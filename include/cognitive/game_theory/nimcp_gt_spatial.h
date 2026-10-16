//=============================================================================
// nimcp_gt_spatial.h - Population and Spatial Games with Evolutionary Dynamics
//=============================================================================
/**
 * @file nimcp_gt_spatial.h
 * @brief Evolutionary game theory with spatial structure and replicator dynamics
 *
 * WHAT: Population-level strategy evolution on networks and lattices
 * WHY:  Model local interactions, spatial effects, and evolutionary stability
 * HOW:  Replicator dynamics, imitation, best-response on various topologies
 *
 * BIOLOGICAL INSPIRATION:
 * - Territorial competition in neural circuits
 * - Spatial spread of neural activity patterns
 * - Local synaptic competition and cooperation
 * - Cortical column interactions on lattice structures
 * - Epidemic-like spread of activation in brain networks
 *
 * KEY CONCEPTS:
 * - Replicator dynamics: dx_i/dt = x_i * (f_i - avg_fitness)
 * - Evolutionarily stable strategy (ESS): resistant to invasion
 * - Spatial structure: local interactions change dynamics
 * - Invasion fitness: can a mutant spread in a resident population?
 *
 * ERROR CODE RANGE: 25000-25999 (Shared with game theory module)
 * BIO-ASYNC MODULE ID: 0x150A
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_SPATIAL_H
#define NIMCP_GT_SPATIAL_H

#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Module-Specific Error Codes
//=============================================================================

#define NIMCP_SPATIAL_ERROR_INVALID_TOPOLOGY    (NIMCP_GT_ERROR_BASE + 60)
#define NIMCP_SPATIAL_ERROR_NETWORK_TOO_LARGE   (NIMCP_GT_ERROR_BASE + 61)
#define NIMCP_SPATIAL_ERROR_NODE_OUT_OF_BOUNDS  (NIMCP_GT_ERROR_BASE + 62)
#define NIMCP_SPATIAL_ERROR_INVALID_STRATEGY    (NIMCP_GT_ERROR_BASE + 63)
#define NIMCP_SPATIAL_ERROR_NO_CONVERGENCE      (NIMCP_GT_ERROR_BASE + 64)
#define NIMCP_SPATIAL_ERROR_INVALID_NETWORK     (NIMCP_GT_ERROR_BASE + 65)
#define NIMCP_SPATIAL_ERROR_SIMULATION_RUNNING  (NIMCP_GT_ERROR_BASE + 66)

//=============================================================================
// Bio-Async Module ID
//=============================================================================

#define BIO_MODULE_GT_SPATIAL   (BIO_MODULE_GAME_THEORY_BASE + 0xA)

//=============================================================================
// Tier-Scaled Constants
//=============================================================================

/** Maximum nodes in spatial game (tier-scaled) */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_SPATIAL_MAX_NODES         10000
    #define NIMCP_SPATIAL_MAX_NEIGHBORS     100
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_SPATIAL_MAX_NODES         2500
    #define NIMCP_SPATIAL_MAX_NEIGHBORS     50
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_SPATIAL_MAX_NODES         625
    #define NIMCP_SPATIAL_MAX_NEIGHBORS     25
#else
    #define NIMCP_SPATIAL_MAX_NODES         100
    #define NIMCP_SPATIAL_MAX_NEIGHBORS     10
#endif

/** Maximum strategies */
#define NIMCP_SPATIAL_MAX_STRATEGIES    16

/** Default simulation parameters */
#define NIMCP_SPATIAL_DEFAULT_GRID_SIZE     10
#define NIMCP_SPATIAL_DEFAULT_STEPS         1000
#define NIMCP_SPATIAL_DEFAULT_DT            0.01f
#define NIMCP_SPATIAL_DEFAULT_SELECTION     1.0f
#define NIMCP_SPATIAL_DEFAULT_MUTATION      0.001f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Network topology types
 *
 * BIOLOGICAL ANALOGY:
 * - COMPLETE: Global workspace broadcast (all regions connected)
 * - GRID_2D: Retinotopic/tonotopic maps (local neighbors)
 * - RING: Circular processing loops (basal ganglia)
 * - SCALE_FREE: Hub-dominated networks (some regions highly connected)
 */
typedef enum {
    NIMCP_TOPOLOGY_COMPLETE,         /**< Complete graph: all connected (n*(n-1)/2 edges) */
    NIMCP_TOPOLOGY_GRID_2D,          /**< 2D lattice with 4 neighbors (von Neumann) */
    NIMCP_TOPOLOGY_GRID_2D_MOORE,    /**< 2D lattice with 8 neighbors (Moore) */
    NIMCP_TOPOLOGY_RING,             /**< Ring/circular graph */
    NIMCP_TOPOLOGY_RANDOM_GRAPH,     /**< Erdos-Renyi random graph */
    NIMCP_TOPOLOGY_SCALE_FREE,       /**< Barabasi-Albert preferential attachment */
    NIMCP_TOPOLOGY_SMALL_WORLD,      /**< Watts-Strogatz small-world network */
    NIMCP_TOPOLOGY_CUSTOM,           /**< User-provided adjacency matrix */
    NIMCP_TOPOLOGY_COUNT
} nimcp_topology_type_t;

/**
 * @brief Strategy update rules
 *
 * BIOLOGICAL ANALOGY:
 * - REPLICATOR: Population-level selection (differential reproduction)
 * - IMITATION: Social learning (copy successful neighbors)
 * - BEST_RESPONSE: Local adaptation (optimize given neighbors)
 * - FERMI: Noisy selection (stochastic update with temperature)
 */
typedef enum {
    NIMCP_UPDATE_REPLICATOR,         /**< Replicator dynamics: dx/dt = x(f - avg_f) */
    NIMCP_UPDATE_IMITATION,          /**< Imitate best-performing neighbor */
    NIMCP_UPDATE_BEST_RESPONSE,      /**< Switch to best response against neighbors */
    NIMCP_UPDATE_FERMI,              /**< Fermi update: probabilistic based on fitness diff */
    NIMCP_UPDATE_MORAN,              /**< Moran process: proportional selection */
    NIMCP_UPDATE_DEATH_BIRTH,        /**< Death-birth: random death, fitness-prop birth */
    NIMCP_UPDATE_COUNT
} nimcp_update_rule_t;

/**
 * @brief Boundary conditions for grid topologies
 */
typedef enum {
    NIMCP_BOUNDARY_PERIODIC,         /**< Toroidal (wrap around) */
    NIMCP_BOUNDARY_FIXED,            /**< Fixed boundary (edge nodes have fewer neighbors) */
    NIMCP_BOUNDARY_REFLECTIVE        /**< Reflective boundary */
} nimcp_boundary_type_t;

/**
 * @brief Simulation state
 */
typedef enum {
    NIMCP_SPATIAL_STATE_UNINITIALIZED,
    NIMCP_SPATIAL_STATE_INITIALIZED,
    NIMCP_SPATIAL_STATE_RUNNING,
    NIMCP_SPATIAL_STATE_CONVERGED,
    NIMCP_SPATIAL_STATE_STOPPED
} nimcp_spatial_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Spatial game configuration
 */
typedef struct {
    /** Topology configuration */
    nimcp_topology_type_t topology;      /**< Network topology type */
    uint32_t grid_width;                 /**< Grid width (for 2D topologies) */
    uint32_t grid_height;                /**< Grid height (for 2D topologies) */
    uint32_t num_nodes;                  /**< Total number of nodes */
    nimcp_boundary_type_t boundary;      /**< Boundary conditions */

    /** Random graph parameters */
    float edge_probability;              /**< For Erdos-Renyi: P(edge exists) */
    uint32_t initial_edges;              /**< For scale-free: initial connected nodes */
    uint32_t edges_per_step;             /**< For scale-free: edges added per node */
    float rewiring_prob;                 /**< For small-world: rewiring probability */

    /** Game parameters */
    uint32_t num_strategies;             /**< Number of available strategies */
    float selection_intensity;           /**< Selection strength (beta) */
    float mutation_rate;                 /**< Random mutation probability */

    /** Update rule configuration */
    nimcp_update_rule_t update_rule;     /**< How strategies update */
    bool synchronous_update;             /**< Synchronous (all at once) vs async */
    float fermi_temperature;             /**< Temperature for Fermi updates */
    float dt;                            /**< Time step for replicator dynamics */

    /** Simulation parameters */
    uint32_t max_steps;                  /**< Maximum simulation steps */
    float convergence_threshold;         /**< Frequency change threshold for convergence */
    uint32_t convergence_window;         /**< Steps to check for convergence */

    /** Options */
    bool track_history;                  /**< Track frequency history? */
    bool enable_statistics;              /**< Collect detailed statistics? */
    uint32_t seed;                       /**< Random seed (0 = use time) */
} nimcp_spatial_config_t;

/**
 * @brief Population state (strategy frequencies)
 */
typedef struct {
    float* frequencies;                  /**< Strategy frequencies [num_strategies] */
    uint32_t num_strategies;             /**< Number of strategies */
    float* fitness;                      /**< Current fitness per strategy */
    float avg_fitness;                   /**< Population average fitness */
    uint64_t generation;                 /**< Current generation/step */
} nimcp_population_t;

/**
 * @brief Network structure for spatial game topologies
 *
 * Note: Named nimcp_spatial_network_t to avoid conflict with the main API's
 * nimcp_network_t which is for neural networks.
 */
typedef struct {
    uint32_t num_nodes;                  /**< Number of nodes */
    uint32_t* degree;                    /**< Degree of each node */
    uint32_t** neighbors;                /**< Adjacency lists: neighbors[i][j] */
    float** weights;                     /**< Edge weights (NULL for unweighted) */
    bool is_directed;                    /**< Directed graph? */
} nimcp_spatial_network_t;

/**
 * @brief Evolution result after simulation run
 */
typedef struct {
    nimcp_spatial_state_t final_state;   /**< Final simulation state */
    uint32_t steps_taken;                /**< Steps until convergence/stop */
    float final_frequencies[NIMCP_SPATIAL_MAX_STRATEGIES]; /**< Final strategy frequencies */
    int32_t dominant_strategy;           /**< Index of dominant strategy (-1 if none) */
    float dominance_ratio;               /**< Frequency of dominant strategy */

    /** Equilibrium information */
    bool is_equilibrium;                 /**< At evolutionary equilibrium? */
    bool is_ess;                         /**< Dominant is ESS? */
    float equilibrium_fitness;           /**< Fitness at equilibrium */

    /** Dynamics information */
    float* frequency_history;            /**< History of frequencies [steps * num_strat] */
    uint32_t history_length;             /**< Number of recorded history points */

    /** Statistics */
    float avg_clustering;                /**< Average strategy clustering coefficient */
    float entropy;                       /**< Shannon entropy of final distribution */
    uint64_t strategy_switches;          /**< Total strategy changes during sim */
} nimcp_evolutionary_result_t;

/**
 * @brief Invasion analysis result
 */
typedef struct {
    bool can_invade;                     /**< Can mutant invade resident? */
    float invasion_fitness;              /**< Mutant fitness in resident population */
    float resident_fitness;              /**< Resident fitness */
    float invasion_probability;          /**< Probability of fixation */
    float expected_time_to_fixation;     /**< Expected steps to fixation (if successful) */
} nimcp_invasion_result_t;

/**
 * @brief Opaque spatial game handle
 */
typedef struct nimcp_spatial_game_struct* nimcp_spatial_game_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default spatial game configuration
 *
 * WHAT: Returns sensible defaults for spatial evolutionary games
 * WHY:  Easy setup with reasonable parameters
 * HOW:  10x10 grid, 2 strategies, replicator dynamics
 *
 * @return Default configuration
 */
nimcp_spatial_config_t nimcp_spatial_default_config(void);

/**
 * @brief Create spatial game context
 *
 * WHAT: Initialize spatial game with payoff matrix
 * WHY:  Set up evolutionary dynamics on network
 * HOW:  Allocate network, initialize nodes with random strategies
 *
 * @param config Game configuration
 * @param payoff_matrix Payoff matrix [num_strategies x num_strategies]
 *                      payoff_matrix[i*n + j] = payoff when i meets j
 * @return Game handle or NULL on failure
 */
nimcp_spatial_game_t nimcp_spatial_create(
    const nimcp_spatial_config_t* config,
    const float* payoff_matrix
);

/**
 * @brief Destroy spatial game context
 *
 * WHAT: Release all resources
 * WHY:  Clean memory management
 * HOW:  Free network, node states, history
 *
 * @param ctx Game context
 */
void nimcp_spatial_destroy(nimcp_spatial_game_t ctx);

//=============================================================================
// Topology Configuration
//=============================================================================

/**
 * @brief Set network topology with parameters
 *
 * WHAT: Configure the interaction network
 * WHY:  Define who interacts with whom
 * HOW:  Generate appropriate network structure
 *
 * @param ctx Game context
 * @param topology Topology type
 * @param params Topology-specific parameters (can be NULL for defaults):
 *               - RANDOM_GRAPH: params[0] = edge_probability
 *               - SCALE_FREE: params[0] = initial_edges, params[1] = edges_per_step
 *               - SMALL_WORLD: params[0] = rewiring_probability
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_spatial_set_topology(
    nimcp_spatial_game_t ctx,
    nimcp_topology_type_t topology,
    const float* params
);

/**
 * @brief Set custom network from adjacency matrix
 *
 * WHAT: Use user-provided network structure
 * WHY:  Support arbitrary topologies
 * HOW:  Parse adjacency and build neighbor lists
 *
 * @param ctx Game context
 * @param adjacency Adjacency matrix [num_nodes x num_nodes] (row-major)
 *                  Non-zero = edge exists
 * @param num_nodes Number of nodes in network
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_spatial_set_custom_network(
    nimcp_spatial_game_t ctx,
    const float* adjacency,
    uint32_t num_nodes
);

/**
 * @brief Get current network structure
 *
 * @param ctx Game context
 * @param network Output network structure (caller must free with nimcp_spatial_network_destroy)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_spatial_get_network(
    const nimcp_spatial_game_t ctx,
    nimcp_spatial_network_t* network
);

/**
 * @brief Destroy spatial network structure
 *
 * @param network Network to destroy
 */
void nimcp_spatial_network_destroy(nimcp_spatial_network_t* network);

//=============================================================================
// Initialization Functions
//=============================================================================

/**
 * @brief Initialize with random strategy distribution
 *
 * WHAT: Randomly assign strategies to nodes
 * WHY:  Start simulation with mixed population
 * HOW:  Each node gets strategy i with probability strategy_probs[i]
 *
 * @param ctx Game context
 * @param strategy_probs Probability for each strategy [num_strategies]
 *                       Must sum to 1.0. NULL = uniform distribution.
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_spatial_initialize_random(
    nimcp_spatial_game_t ctx,
    const float* strategy_probs
);

/**
 * @brief Initialize with strategy cluster
 *
 * WHAT: Place a cluster of one strategy in network
 * WHY:  Test invasion scenarios, study spatial spread
 * HOW:  Set nodes within radius of center to given strategy
 *
 * For grid: center is (center % width, center / width), radius is Euclidean
 * For graphs: center is node index, radius is graph distance
 *
 * @param ctx Game context
 * @param strategy Strategy index for cluster
 * @param center Center node index
 * @param radius Cluster radius
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_spatial_initialize_cluster(
    nimcp_spatial_game_t ctx,
    uint32_t strategy,
    uint32_t center,
    float radius
);

/**
 * @brief Set strategy for a specific node
 *
 * @param ctx Game context
 * @param node_id Node index
 * @param strategy Strategy index
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_spatial_set_node_strategy(
    nimcp_spatial_game_t ctx,
    uint32_t node_id,
    uint32_t strategy
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Execute one evolution step
 *
 * WHAT: Advance simulation by one time step
 * WHY:  Fine-grained control over evolution
 * HOW:  Apply update rule to each node
 *
 * @param ctx Game context
 * @return NIMCP_SUCCESS or error (e.g., already converged)
 */
nimcp_error_t nimcp_spatial_step(nimcp_spatial_game_t ctx);

/**
 * @brief Run simulation for multiple steps
 *
 * WHAT: Execute evolution until convergence or max steps
 * WHY:  Find equilibrium of spatial game
 * HOW:  Iterate steps, check convergence, collect results
 *
 * @param ctx Game context
 * @param num_steps Maximum steps (0 = use config max_steps)
 * @param result Output evolution result (can be NULL)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_spatial_run(
    nimcp_spatial_game_t ctx,
    uint32_t num_steps,
    nimcp_evolutionary_result_t* result
);

/**
 * @brief Stop a running simulation
 *
 * @param ctx Game context
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_spatial_stop(nimcp_spatial_game_t ctx);

/**
 * @brief Reset simulation to initial state
 *
 * @param ctx Game context
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_spatial_reset(nimcp_spatial_game_t ctx);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current strategy frequencies
 *
 * WHAT: Return proportion of population using each strategy
 * WHY:  Track evolution progress
 * HOW:  Count strategies across nodes, normalize
 *
 * @param ctx Game context
 * @param frequencies Output array [num_strategies] (caller allocated)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_spatial_get_frequencies(
    const nimcp_spatial_game_t ctx,
    float* frequencies
);

/**
 * @brief Get strategy of a specific node
 *
 * @param ctx Game context
 * @param node_id Node index
 * @return Strategy index or -1 on error
 */
int32_t nimcp_spatial_get_node_strategy(
    const nimcp_spatial_game_t ctx,
    uint32_t node_id
);

/**
 * @brief Get node fitness
 *
 * @param ctx Game context
 * @param node_id Node index
 * @return Node fitness or NaN on error
 */
float nimcp_spatial_get_node_fitness(
    const nimcp_spatial_game_t ctx,
    uint32_t node_id
);

/**
 * @brief Get current simulation state
 *
 * @param ctx Game context
 * @return Current state
 */
nimcp_spatial_state_t nimcp_spatial_get_state(const nimcp_spatial_game_t ctx);

/**
 * @brief Get current step count
 *
 * @param ctx Game context
 * @return Current step number
 */
uint32_t nimcp_spatial_get_step(const nimcp_spatial_game_t ctx);

/**
 * @brief Get population state
 *
 * @param ctx Game context
 * @param population Output population state
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_spatial_get_population(
    const nimcp_spatial_game_t ctx,
    nimcp_population_t* population
);

//=============================================================================
// Evolutionary Stability Analysis
//=============================================================================

/**
 * @brief Check if strategy is evolutionarily stable (ESS)
 *
 * WHAT: Test if strategy resists invasion by any mutant
 * WHY:  ESS is key concept in evolutionary game theory
 * HOW:  Check: E(s,s) > E(s',s) or [E(s,s) = E(s',s) and E(s,s') > E(s',s')]
 *
 * @param ctx Game context
 * @param strategy Strategy index to test
 * @return true if ESS
 */
bool nimcp_spatial_is_ess(
    const nimcp_spatial_game_t ctx,
    uint32_t strategy
);

/**
 * @brief Compute invasion fitness of mutant in resident population
 *
 * WHAT: Calculate fitness of rare mutant strategy
 * WHY:  Predict if mutant can spread
 * HOW:  Expected payoff of mutant against residents
 *
 * FORMULA: f_mutant = Sum_j P(j|neighbors) * payoff(mutant, j)
 * In resident population, P(resident) ~ 1
 *
 * @param ctx Game context
 * @param mutant_strategy Invading strategy
 * @param resident_strategy Established strategy
 * @return Invasion fitness (mutant fitness - resident fitness)
 */
float nimcp_spatial_invasion_fitness(
    const nimcp_spatial_game_t ctx,
    uint32_t mutant_strategy,
    uint32_t resident_strategy
);

/**
 * @brief Analyze invasion dynamics
 *
 * WHAT: Detailed analysis of mutant invasion
 * WHY:  Predict evolutionary outcomes
 * HOW:  Compute fixation probabilities and invasion fitness
 *
 * @param ctx Game context
 * @param mutant_strategy Invading strategy
 * @param resident_strategy Established strategy
 * @param result Output invasion analysis
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_spatial_analyze_invasion(
    const nimcp_spatial_game_t ctx,
    uint32_t mutant_strategy,
    uint32_t resident_strategy,
    nimcp_invasion_result_t* result
);

/**
 * @brief Find evolutionary equilibrium
 *
 * WHAT: Search for stable strategy distribution
 * WHY:  Predict long-term evolutionary outcome
 * HOW:  Run dynamics until convergence
 *
 * @param ctx Game context
 * @param result Output evolution result
 * @return NIMCP_SUCCESS or error (NO_CONVERGENCE if failed)
 */
nimcp_error_t nimcp_spatial_find_equilibrium(
    nimcp_spatial_game_t ctx,
    nimcp_evolutionary_result_t* result
);

//=============================================================================
// Replicator Dynamics (Population-Level)
//=============================================================================

/**
 * @brief Compute one step of replicator dynamics
 *
 * WHAT: Update frequencies according to replicator equation
 * WHY:  Model selection without spatial structure
 * HOW:  dx_i/dt = x_i * (f_i - avg_fitness)
 *
 * REPLICATOR EQUATION:
 *   f_i = Sum_j x_j * payoff(i, j)       // fitness of strategy i
 *   avg_f = Sum_i x_i * f_i              // average fitness
 *   x_i(t+dt) = x_i(t) + dt * x_i * (f_i - avg_f)
 *
 * @param frequencies Current strategy frequencies [num_strategies]
 * @param payoff_matrix Payoff matrix [n x n, row-major]
 * @param num_strategies Number of strategies
 * @param dt Time step
 * @param new_frequencies Output frequencies [num_strategies] (can be same as input)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_replicator_dynamics(
    const float* frequencies,
    const float* payoff_matrix,
    uint32_t num_strategies,
    float dt,
    float* new_frequencies
);

/**
 * @brief Compute fitness for each strategy
 *
 * WHAT: Calculate expected payoff per strategy
 * WHY:  Core of replicator dynamics
 * HOW:  f_i = Sum_j x_j * payoff(i, j)
 *
 * @param frequencies Strategy frequencies
 * @param payoff_matrix Payoff matrix
 * @param num_strategies Number of strategies
 * @param fitness Output fitness array [num_strategies]
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_compute_strategy_fitness(
    const float* frequencies,
    const float* payoff_matrix,
    uint32_t num_strategies,
    float* fitness
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get topology type name
 */
const char* nimcp_topology_name(nimcp_topology_type_t topology);

/**
 * @brief Get update rule name
 */
const char* nimcp_update_rule_name(nimcp_update_rule_t rule);

/**
 * @brief Compute Shannon entropy of distribution
 *
 * @param frequencies Probability distribution
 * @param n Number of elements
 * @return Entropy in bits
 */
float nimcp_compute_entropy(const float* frequencies, uint32_t n);

/**
 * @brief Free evolution result resources
 *
 * @param result Result to clean up
 */
void nimcp_evolutionary_result_cleanup(nimcp_evolutionary_result_t* result);

/**
 * @brief Free population resources
 *
 * @param population Population to clean up
 */
void nimcp_population_cleanup(nimcp_population_t* population);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_SPATIAL_H
