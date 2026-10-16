/**
 * @file nimcp_parietal_quantum_bridge.h
 * @brief Quantum-accelerated parietal lobe reasoning
 *
 * WHAT: Integrates quantum algorithms with parietal lobe for physics/engineering
 * WHY:  Quantum speedup for optimization, search, and simulation problems
 * HOW:  Bridges quantum annealing, quantum walk, and quantum reasoning
 *
 * CAPABILITIES:
 * - Quantum annealing for engineering optimization (design, control, structure)
 * - Quantum walk for solution space exploration
 * - Quantum-accelerated linear algebra for physics simulation
 * - VQE-inspired variational algorithms for molecular/materials simulation
 * - QAOA for combinatorial optimization problems
 *
 * BIOLOGICAL INSPIRATION:
 * The parietal cortex integrates multiple modalities for spatial reasoning.
 * Quantum superposition provides parallel exploration of solution spaces,
 * analogous to how the brain considers multiple interpretations simultaneously.
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_PARIETAL_QUANTUM_BRIDGE_H
#define NIMCP_PARIETAL_QUANTUM_BRIDGE_H

/* Forward declaration to avoid circular include with nimcp_parietal.h */
#ifndef NIMCP_PARIETAL_LOBE_T_DEFINED
#define NIMCP_PARIETAL_LOBE_T_DEFINED
typedef struct parietal_lobe parietal_lobe_t;
#endif

#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"
#include "utils/quantum/nimcp_quantum_walk.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Bio-async module ID */
#define BIO_MODULE_PARIETAL_QUANTUM     0x03A0

/** Maximum qubits for simulation */
#define PARIETAL_QUANTUM_MAX_QUBITS     16

/** Maximum QAOA layers */
#define PARIETAL_QUANTUM_MAX_QAOA_P     10

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for parietal quantum bridge */
typedef struct parietal_quantum_bridge parietal_quantum_bridge_t;

/**
 * @brief Quantum algorithm selection
 */
typedef enum {
    PARIETAL_QA_ANNEALING,              /**< Quantum annealing for optimization */
    PARIETAL_QA_QAOA,                   /**< QAOA for combinatorial problems */
    PARIETAL_QA_VQE,                    /**< VQE for eigenvalue problems */
    PARIETAL_QA_GROVER,                 /**< Grover search for solution finding */
    PARIETAL_QA_QUANTUM_WALK,           /**< Quantum walk for graph problems */
    PARIETAL_QA_HHL                     /**< HHL for linear systems (future) */
} parietal_quantum_algorithm_t;

/**
 * @brief Optimization problem types
 */
typedef enum {
    PARIETAL_OPT_MINIMIZE,              /**< Minimize objective */
    PARIETAL_OPT_MAXIMIZE,              /**< Maximize objective */
    PARIETAL_OPT_FEASIBILITY,           /**< Find feasible solution */
    PARIETAL_OPT_PARETO                 /**< Multi-objective Pareto front */
} parietal_opt_type_t;

/**
 * @brief Engineering optimization problem
 */
typedef struct parietal_opt_problem_s {
    float* variables;                   /**< Decision variables */
    uint32_t num_variables;             /**< Number of variables */
    float* lower_bounds;                /**< Lower bounds */
    float* upper_bounds;                /**< Upper bounds */

    /* Objective function (user-provided) */
    float (*objective)(const float* x, void* ctx);
    void* objective_ctx;

    /* Constraints (optional) */
    float (*constraints)(const float* x, uint32_t idx, void* ctx);
    uint32_t num_constraints;
    void* constraint_ctx;

    parietal_opt_type_t type;
    char description[128];
} parietal_opt_problem_t;

/**
 * @brief Optimization result
 */
typedef struct parietal_opt_result_s {
    float* optimal_variables;           /**< Optimal solution */
    uint32_t num_variables;
    float optimal_value;                /**< Objective value at optimum */
    float* constraint_values;           /**< Constraint values at optimum */
    bool is_feasible;                   /**< Solution satisfies constraints */
    float quantum_advantage;            /**< Estimated speedup over classical */
    uint32_t iterations;                /**< Iterations to converge */
    float confidence;                   /**< Solution confidence [0,1] */
} parietal_opt_result_t;

/**
 * @brief Linear system for quantum solving
 */
typedef struct {
    float** A;                          /**< Matrix A [n x n] */
    float* b;                           /**< Vector b [n] */
    uint32_t n;                         /**< System size */
    float condition_number;             /**< Matrix condition number */
} parietal_linear_system_t;

/**
 * @brief Linear system result
 */
typedef struct {
    float* x;                           /**< Solution vector */
    uint32_t n;
    float residual_norm;                /**< ||Ax - b|| */
    float quantum_speedup;              /**< O(log n) vs O(n^3) estimate */
    bool converged;
} parietal_linear_result_t;

/**
 * @brief Hamiltonian for physics simulation
 */
typedef struct parietal_hamiltonian_s {
    float** matrix;                     /**< Hamiltonian matrix */
    uint32_t dim;                       /**< Hilbert space dimension */
    bool is_sparse;                     /**< Sparse representation */
    uint32_t* sparse_row;               /**< CSR row pointers (if sparse) */
    uint32_t* sparse_col;               /**< CSR column indices */
    float* sparse_val;                  /**< CSR values */
    uint32_t nnz;                       /**< Number of non-zeros */
} parietal_hamiltonian_t;

/**
 * @brief VQE result for eigenvalue problems
 */
typedef struct parietal_vqe_result_s {
    float ground_energy;                /**< Ground state energy */
    float* ground_state;                /**< Ground state vector (optional) */
    uint32_t dim;
    float* excited_energies;            /**< Excited state energies (optional) */
    uint32_t num_excited;
    uint32_t vqe_iterations;
    float chemical_accuracy;            /**< Error in Hartree */
} parietal_vqe_result_t;

/**
 * @brief Graph structure for quantum walk
 */
typedef struct {
    uint32_t num_nodes;
    uint32_t* adjacency_row;            /**< CSR format */
    uint32_t* adjacency_col;
    float* edge_weights;
    uint32_t num_edges;
} parietal_graph_t;

/**
 * @brief Quantum walk result
 */
typedef struct {
    float* node_probabilities;          /**< Probability at each node */
    uint32_t num_nodes;
    float hitting_time;                 /**< Expected hitting time */
    float mixing_time;                  /**< Time to stationary */
    uint32_t* path;                     /**< Path found (for search) */
    uint32_t path_length;
} parietal_walk_result_t;

/**
 * @brief Bridge configuration
 */
typedef struct parietal_quantum_config_s {
    bool enabled;
    parietal_quantum_algorithm_t default_algorithm;

    /* Annealing settings */
    float annealing_temperature;
    float quantum_strength;
    uint32_t annealing_iterations;

    /* QAOA settings */
    uint32_t qaoa_layers;
    float qaoa_gamma_init;
    float qaoa_beta_init;

    /* VQE settings */
    uint32_t vqe_max_iterations;
    float vqe_convergence;

    /* General */
    uint32_t max_qubits;
    bool use_noise_model;
    float error_rate;
    bool enable_error_mitigation;

    /* Modulation */
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} parietal_quantum_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t optimizations_run;
    uint64_t linear_systems_solved;
    uint64_t vqe_runs;
    uint64_t quantum_walks;
    float avg_speedup;
    float total_quantum_time_us;
    uint64_t classical_fallbacks;
} parietal_quantum_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
parietal_quantum_config_t parietal_quantum_default_config(void);

/**
 * @brief Create parietal quantum bridge
 */
parietal_quantum_bridge_t* parietal_quantum_bridge_create(
    const parietal_quantum_config_t* config
);

/**
 * @brief Destroy bridge
 */
void parietal_quantum_bridge_destroy(parietal_quantum_bridge_t* bridge);

/**
 * @brief Enable/disable quantum acceleration
 */
int parietal_quantum_set_enabled(parietal_quantum_bridge_t* bridge, bool enabled);

/**
 * @brief Check if quantum is available
 */
bool parietal_quantum_is_available(const parietal_quantum_bridge_t* bridge);

/* ============================================================================
 * OPTIMIZATION API
 * ============================================================================ */

/**
 * @brief Solve optimization problem using quantum annealing/QAOA
 *
 * Uses quantum-inspired annealing for continuous problems,
 * QAOA for combinatorial problems.
 *
 * @param bridge Quantum bridge handle
 * @param problem Optimization problem definition
 * @param result Output result
 * @return 0 on success
 */
int parietal_quantum_optimize(
    parietal_quantum_bridge_t* bridge,
    const parietal_opt_problem_t* problem,
    parietal_opt_result_t* result
);

/**
 * @brief Solve QUBO (Quadratic Unconstrained Binary Optimization)
 *
 * QUBO is natural form for quantum annealing.
 * min x^T Q x, x ∈ {0,1}^n
 *
 * @param bridge Quantum bridge handle
 * @param Q QUBO matrix [n x n]
 * @param n Problem size
 * @param result Output binary solution [n]
 * @param energy Output minimum energy
 * @return 0 on success
 */
int parietal_quantum_solve_qubo(
    parietal_quantum_bridge_t* bridge,
    const float** Q,
    uint32_t n,
    uint8_t* result,
    float* energy
);

/**
 * @brief Solve MaxCut problem
 *
 * Classic NP-hard problem suitable for QAOA.
 *
 * @param bridge Quantum bridge handle
 * @param graph Graph structure
 * @param partition Output node partition [num_nodes]
 * @param cut_value Output cut value
 * @return 0 on success
 */
int parietal_quantum_maxcut(
    parietal_quantum_bridge_t* bridge,
    const parietal_graph_t* graph,
    uint8_t* partition,
    float* cut_value
);

/**
 * @brief Free optimization result
 */
void parietal_quantum_free_opt_result(parietal_opt_result_t* result);

/* ============================================================================
 * LINEAR ALGEBRA API
 * ============================================================================ */

/**
 * @brief Solve linear system Ax = b using quantum-inspired methods
 *
 * Uses HHL-inspired algorithm for potential exponential speedup
 * when A is well-conditioned and sparse.
 *
 * @param bridge Quantum bridge handle
 * @param system Linear system
 * @param result Output solution
 * @return 0 on success
 */
int parietal_quantum_solve_linear(
    parietal_quantum_bridge_t* bridge,
    const parietal_linear_system_t* system,
    parietal_linear_result_t* result
);

/**
 * @brief Compute eigenvalues using quantum-inspired methods
 *
 * @param bridge Quantum bridge handle
 * @param matrix Matrix [n x n]
 * @param n Matrix size
 * @param num_eigenvalues Number of eigenvalues to compute
 * @param eigenvalues Output eigenvalues
 * @return 0 on success
 */
int parietal_quantum_eigenvalues(
    parietal_quantum_bridge_t* bridge,
    const float** matrix,
    uint32_t n,
    uint32_t num_eigenvalues,
    float* eigenvalues
);

/**
 * @brief Free linear result
 */
void parietal_quantum_free_linear_result(parietal_linear_result_t* result);

/* ============================================================================
 * PHYSICS SIMULATION API (VQE)
 * ============================================================================ */

/**
 * @brief Find ground state energy using VQE
 *
 * Variational Quantum Eigensolver for chemistry/materials simulation.
 *
 * @param bridge Quantum bridge handle
 * @param hamiltonian System Hamiltonian
 * @param result VQE result
 * @return 0 on success
 */
int parietal_quantum_vqe(
    parietal_quantum_bridge_t* bridge,
    const parietal_hamiltonian_t* hamiltonian,
    parietal_vqe_result_t* result
);

/**
 * @brief Simulate time evolution exp(-iHt)|ψ⟩
 *
 * @param bridge Quantum bridge handle
 * @param hamiltonian System Hamiltonian
 * @param initial_state Initial state vector
 * @param time Evolution time
 * @param final_state Output final state
 * @return 0 on success
 */
int parietal_quantum_time_evolution(
    parietal_quantum_bridge_t* bridge,
    const parietal_hamiltonian_t* hamiltonian,
    const float* initial_state,
    float time,
    float* final_state
);

/**
 * @brief Free VQE result
 */
void parietal_quantum_free_vqe_result(parietal_vqe_result_t* result);

/* ============================================================================
 * QUANTUM WALK API
 * ============================================================================ */

/**
 * @brief Perform quantum walk on graph
 *
 * @param bridge Quantum bridge handle
 * @param graph Graph structure
 * @param start_node Starting node
 * @param num_steps Number of walk steps
 * @param result Walk result
 * @return 0 on success
 */
int parietal_quantum_walk(
    parietal_quantum_bridge_t* bridge,
    const parietal_graph_t* graph,
    uint32_t start_node,
    uint32_t num_steps,
    parietal_walk_result_t* result
);

/**
 * @brief Quantum walk search for marked node
 *
 * Grover-walk hybrid for O(√N) search on graphs.
 *
 * @param bridge Quantum bridge handle
 * @param graph Graph structure
 * @param is_marked Function returning true for marked nodes
 * @param ctx User context for is_marked
 * @param result Walk/search result
 * @return 0 on success
 */
int parietal_quantum_walk_search(
    parietal_quantum_bridge_t* bridge,
    const parietal_graph_t* graph,
    bool (*is_marked)(uint32_t node, void* ctx),
    void* ctx,
    parietal_walk_result_t* result
);

/**
 * @brief Free walk result
 */
void parietal_quantum_free_walk_result(parietal_walk_result_t* result);

/* ============================================================================
 * ENGINEERING APPLICATIONS
 * ============================================================================ */

/**
 * @brief Structural topology optimization
 *
 * Find optimal material distribution using quantum optimization.
 *
 * @param bridge Quantum bridge handle
 * @param domain Domain discretization [nx * ny * nz]
 * @param nx, ny, nz Domain dimensions
 * @param loads Applied loads
 * @param num_loads Number of loads
 * @param volume_fraction Target volume fraction [0,1]
 * @param density Output material density [nx * ny * nz]
 * @return 0 on success
 */
int parietal_quantum_topology_opt(
    parietal_quantum_bridge_t* bridge,
    const float* domain,
    uint32_t nx, uint32_t ny, uint32_t nz,
    const float* loads,
    uint32_t num_loads,
    float volume_fraction,
    float* density
);

/**
 * @brief Circuit optimization
 *
 * Optimize circuit parameters using quantum annealing.
 *
 * @param bridge Quantum bridge handle
 * @param num_components Number of components
 * @param component_values Component values (modified in place)
 * @param objective Objective function (minimize)
 * @param ctx User context
 * @return 0 on success
 */
int parietal_quantum_circuit_opt(
    parietal_quantum_bridge_t* bridge,
    uint32_t num_components,
    float* component_values,
    float (*objective)(const float* values, void* ctx),
    void* ctx
);

/**
 * @brief Control system optimization (PID tuning, etc.)
 *
 * @param bridge Quantum bridge handle
 * @param num_params Number of controller parameters
 * @param params Controller parameters (modified)
 * @param simulate Simulation function returning performance metric
 * @param ctx User context
 * @return 0 on success
 */
int parietal_quantum_control_opt(
    parietal_quantum_bridge_t* bridge,
    uint32_t num_params,
    float* params,
    float (*simulate)(const float* params, void* ctx),
    void* ctx
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int parietal_quantum_set_inflammation(parietal_quantum_bridge_t* bridge, float level);
int parietal_quantum_set_fatigue(parietal_quantum_bridge_t* bridge, float level);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int parietal_quantum_get_stats(
    const parietal_quantum_bridge_t* bridge,
    parietal_quantum_stats_t* stats
);

void parietal_quantum_reset_stats(parietal_quantum_bridge_t* bridge);

const char* parietal_quantum_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_QUANTUM_BRIDGE_H */
