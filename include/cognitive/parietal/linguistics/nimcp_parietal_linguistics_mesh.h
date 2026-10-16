/**
 * @file nimcp_parietal_linguistics_mesh.h
 * @brief Linguistics Mesh Coordinator - Distributed Consensus with Advanced Algorithms
 * @version 2.0.0
 * @date 2025-01-31
 *
 * WHAT: Central mesh coordinator for distributed linguistics processing with
 *       advanced mathematical algorithms for belief propagation and consensus
 * WHY:  Enable peer-to-peer communication and convergent consensus across
 *       all parietal linguistics modules using FEP-based belief updates
 * HOW:  Multiple convergence algorithms + BBB + Exception + Health + KG integration
 *
 * MATHEMATICAL ALGORITHMS:
 * 1. Sum-Product (Belief Propagation) - Exact inference on trees
 * 2. Loopy Belief Propagation - Approximate inference with damping
 * 3. Weighted Average Consensus - Distributed averaging
 * 4. Metropolis-Hastings Weights - Optimal gossip matrix
 * 5. Graph Laplacian Consensus - Spectral convergence
 * 6. Natural Gradient Descent - Fisher Information Matrix
 * 7. Covariance Intersection - Handles unknown correlations
 * 8. KL Divergence - Information-theoretic agreement
 * 9. Trimmed Mean/Median - Byzantine fault tolerance
 * 10. Krum Algorithm - Robust gradient aggregation
 * 11. PageRank - Dynamic credibility weighting
 * 12. Lyapunov Stability - Convergence guarantee
 *
 * INFRASTRUCTURE INTEGRATION:
 * - Exception Handling: NIMCP_CHECK_THROW_IMMUNE for all errors
 * - BBB: Input validation, memory protection, access control
 * - Health Monitor: Real-time metrics, anomaly detection
 * - Heartbeat: Liveness detection and watchdog
 * - KG Wiring: Module registration, edge connections
 * - Full Logging: Structured JSON, module-specific levels
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PARIETAL_LINGUISTICS_MESH_H
#define NIMCP_PARIETAL_LINGUISTICS_MESH_H

#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Default neuromodulator channel for fast attention */
#define LING_MESH_CHANNEL_ATTENTION         0x01

/** Default neuromodulator channel for reward signals */
#define LING_MESH_CHANNEL_REWARD            0x02

/** Default neuromodulator channel for priority alerts */
#define LING_MESH_CHANNEL_PRIORITY          0x03

/** Default neuromodulator channel for slow context */
#define LING_MESH_CHANNEL_CONTEXT           0x04

/** Default convergence window (iterations before checking) */
#define LING_MESH_DEFAULT_CONVERGENCE_WINDOW    10

/** Default voting fallback topic ID */
#define LING_MESH_VOTING_TOPIC_INTERPRETATION   0x1001

/** Maximum input string length for BBB validation */
#define LING_MESH_MAX_INPUT_LENGTH          1024

/** Health monitor brain ID for linguistics mesh */
#define LING_MESH_HEALTH_BRAIN_ID           "linguistics_mesh"

/** Heartbeat interval in milliseconds */
#define LING_MESH_HEARTBEAT_INTERVAL_MS     100

/** Maximum missed heartbeats before recovery */
#define LING_MESH_MAX_MISSED_HEARTBEATS     5

/** KG node name for mesh coordinator */
#define LING_MESH_KG_NODE_NAME              "parietal_linguistics_mesh"

/* Algorithm-specific constants */
#define LING_MESH_BP_MAX_ITERATIONS         50      /**< Max belief propagation iterations */
#define LING_MESH_BP_DAMPING_FACTOR         0.5f    /**< Loopy BP damping (0-1) */
#define LING_MESH_PAGERANK_DAMPING          0.85f   /**< PageRank damping factor */
#define LING_MESH_NATURAL_GRAD_EPSILON      1e-6f   /**< Fisher information regularization */
#define LING_MESH_COV_INTERSECT_OMEGA       0.5f    /**< Covariance intersection weight */
#define LING_MESH_TRIMMED_FRACTION          0.2f    /**< Fraction to trim for Byzantine tolerance */
#define LING_MESH_KRUM_TOLERANCE            0.33f   /**< Krum Byzantine tolerance (f < n/2 - 1) */

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for mesh coordinator */
typedef struct linguistics_mesh linguistics_mesh_t;

/** Opaque handle for BBB system */
typedef struct bbb_system_struct* bbb_system_t;

/** Opaque handle for health monitor */
typedef struct health_monitor_internal* health_monitor_t;

/** Opaque handle for brain KG */
typedef struct brain_kg brain_kg_t;

/** KG node ID */
typedef uint32_t brain_kg_node_id_t;

/**
 * @brief Consensus algorithm selection
 */
typedef enum {
    LING_MESH_ALG_FEP_BASIC = 0,        /**< Basic FEP: μ' = μ - lr × Π × ε */
    LING_MESH_ALG_BELIEF_PROPAGATION,   /**< Sum-product algorithm */
    LING_MESH_ALG_LOOPY_BP,             /**< Loopy BP with damping */
    LING_MESH_ALG_WEIGHTED_CONSENSUS,   /**< Weighted average consensus */
    LING_MESH_ALG_METROPOLIS_HASTINGS,  /**< Metropolis-Hastings weights */
    LING_MESH_ALG_LAPLACIAN_CONSENSUS,  /**< Graph Laplacian consensus */
    LING_MESH_ALG_NATURAL_GRADIENT,     /**< Natural gradient descent */
    LING_MESH_ALG_COVARIANCE_INTERSECT, /**< Covariance intersection */
    LING_MESH_ALG_HYBRID,               /**< Adaptive algorithm selection */
    LING_MESH_ALG_COUNT
} linguistics_mesh_algorithm_t;

/**
 * @brief Agreement metric type
 */
typedef enum {
    LING_MESH_METRIC_COSINE = 0,        /**< Cosine similarity */
    LING_MESH_METRIC_EUCLIDEAN,         /**< Euclidean distance */
    LING_MESH_METRIC_KL_DIVERGENCE,     /**< KL divergence */
    LING_MESH_METRIC_MUTUAL_INFO,       /**< Mutual information */
    LING_MESH_METRIC_COUNT
} linguistics_mesh_metric_t;

/**
 * @brief Byzantine tolerance mode
 */
typedef enum {
    LING_MESH_BFT_NONE = 0,             /**< No Byzantine tolerance */
    LING_MESH_BFT_TRIMMED_MEAN,         /**< Trimmed mean */
    LING_MESH_BFT_TRIMMED_MEDIAN,       /**< Trimmed median */
    LING_MESH_BFT_KRUM,                 /**< Krum algorithm */
    LING_MESH_BFT_VOTING,               /**< Voting fallback */
    LING_MESH_BFT_COUNT
} linguistics_mesh_bft_mode_t;

/**
 * @brief Mesh participant entry
 *
 * Tracks a registered module participating in the linguistics mesh.
 */
typedef struct {
    uint32_t module_id;                     /**< Bio-async module ID */
    char module_name[64];                   /**< Human-readable name */
    linguistics_mesh_handler_t handler;     /**< Handler interface */
    float credibility;                      /**< Trust weight [0,1] */
    float last_precision;                   /**< Last reported precision */
    uint64_t last_update_ms;                /**< Last update timestamp */
    bool active;                            /**< Currently active */

    /* Graph topology */
    uint32_t degree;                        /**< Number of neighbors */
    uint32_t* neighbors;                    /**< Array of neighbor indices */
    float* neighbor_weights;                /**< Edge weights to neighbors */

    /* PageRank credibility */
    float pagerank_score;                   /**< Dynamic credibility via PageRank */

    /* Covariance tracking */
    float* covariance_diag;                 /**< Diagonal covariance elements */

    /* Statistics */
    uint64_t beliefs_contributed;           /**< Total beliefs generated */
    uint64_t updates_received;              /**< FEP updates received */
    float avg_precision;                    /**< Average precision */
    uint64_t error_count;                   /**< Error count for Byzantine detection */
} linguistics_mesh_participant_t;

/**
 * @brief Algorithm-specific configuration
 */
typedef struct {
    /* Belief Propagation */
    uint32_t bp_max_iterations;             /**< Max BP iterations */
    float bp_damping;                       /**< Loopy BP damping factor */
    float bp_convergence_threshold;         /**< BP convergence threshold */

    /* Natural Gradient */
    float fisher_epsilon;                   /**< Fisher matrix regularization */
    bool use_diagonal_fisher;               /**< Use diagonal approximation */

    /* Covariance Intersection */
    float ci_omega;                         /**< CI interpolation weight */
    bool ci_optimize_omega;                 /**< Optimize omega automatically */

    /* PageRank */
    float pagerank_damping;                 /**< PageRank damping factor */
    uint32_t pagerank_iterations;           /**< PageRank iterations */

    /* Graph Laplacian */
    bool laplacian_use_normalized;          /**< Use normalized Laplacian */
    float laplacian_step_size;              /**< Consensus step size */

    /* Byzantine Tolerance */
    linguistics_mesh_bft_mode_t bft_mode;   /**< Byzantine tolerance mode */
    float bft_trim_fraction;                /**< Fraction to trim */
    uint32_t bft_max_faulty;                /**< Max faulty nodes to tolerate */
} linguistics_mesh_algorithm_config_t;

/**
 * @brief Mesh coordinator configuration
 */
typedef struct {
    /* Convergence parameters */
    float agreement_threshold;              /**< Min agreement for convergence (default: 0.75) */
    float convergence_threshold;            /**< Free energy delta for halt (default: 0.001) */
    uint32_t max_iterations;                /**< Max FEP iterations (default: 100) */
    uint32_t convergence_window;            /**< Iterations before checking (default: 10) */

    /* FEP parameters */
    float belief_learning_rate;             /**< μ update step size (default: 0.1) */
    float precision_floor;                  /**< Min precision (default: 0.01) */
    float precision_ceiling;                /**< Max precision (default: 100.0) */

    /* Gossip parameters */
    float gossip_probability;               /**< Propagation prob per round (default: 0.3) */
    float credibility_weight;               /**< Trust weight in updates (default: 0.8) */
    float belief_decay_rate;                /**< Decay per round (default: 0.05) */

    /* Fallback parameters */
    bool enable_voting_fallback;            /**< Use voting on deadlock (default: true) */
    float bft_threshold;                    /**< Byzantine fault tolerance (default: 0.333) */

    /* Algorithm selection */
    linguistics_mesh_algorithm_t algorithm; /**< Primary algorithm (default: FEP_BASIC) */
    linguistics_mesh_metric_t metric;       /**< Agreement metric (default: COSINE) */
    linguistics_mesh_algorithm_config_t alg_config; /**< Algorithm-specific config */

    /* Integration enables */
    bool enable_bio_async;                  /**< Enable bio-async messaging (default: true) */
    bool enable_kg_discovery;               /**< Use KG for participant discovery (default: true) */
    bool enable_statistics;                 /**< Track detailed stats (default: true) */
    bool enable_logging;                    /**< Enable logging (default: true) */

    /* Infrastructure integration */
    bool enable_bbb;                        /**< Enable BBB validation (default: true) */
    bool enable_health_monitor;             /**< Enable health monitoring (default: true) */
    bool enable_heartbeat;                  /**< Enable heartbeat (default: true) */
    bool enable_kg_wiring;                  /**< Enable KG wiring (default: true) */
    bool enable_exception_handling;         /**< Enable exception/immune (default: true) */

    /* Neuromodulator channels */
    uint8_t default_channel;                /**< Default routing channel */

    /* Performance */
    uint32_t timeout_ms;                    /**< Default request timeout (default: 5000) */
    uint32_t heartbeat_interval_ms;         /**< Heartbeat interval (default: 100) */
} linguistics_mesh_config_t;

/**
 * @brief Convergence state during mesh request
 */
typedef struct {
    mesh_convergence_state_t state;         /**< Current state */
    uint32_t iteration;                     /**< Current iteration */
    float free_energy;                      /**< Current free energy F */
    float free_energy_delta;                /**< Change since last iteration */
    float agreement_score;                  /**< Current agreement [0,1] */
    float kl_divergence;                    /**< KL divergence between beliefs */
    float deadlock_score;                   /**< Deadlock likelihood [0,1] */
    float rumination_score;                 /**< Repetition pattern score [0,1] */
    float lyapunov_value;                   /**< Lyapunov function value */
    bool lyapunov_decreasing;               /**< Whether Lyapunov is decreasing */
    bool voting_triggered;                  /**< Whether voting fallback was used */
    linguistics_mesh_algorithm_t algorithm_used; /**< Which algorithm was used */
} linguistics_convergence_state_t;

/**
 * @brief Collective belief from mesh consensus
 *
 * Precision-weighted average of all participant beliefs.
 */
typedef struct {
    char topic[128];                        /**< Belief topic */
    float belief_vector[16];                /**< Converged belief encoding */
    uint32_t vector_dim;                    /**< Vector dimension used */
    float collective_certainty;             /**< Weighted certainty [0,1] */
    float collective_precision;             /**< Combined precision */
    float covariance_trace;                 /**< Trace of covariance (uncertainty) */
    uint32_t contributing_participants;     /**< How many contributed */
    uint32_t byzantine_excluded;            /**< Participants excluded as Byzantine */
} linguistics_collective_belief_t;

/**
 * @brief Extended mesh statistics (internal use)
 *
 * This is the extended version used internally by the mesh coordinator.
 * The simpler linguistics_mesh_stats_t is defined in nimcp_parietal_linguistics_types.h.
 */
typedef struct {
    /* Basic stats (compatible with linguistics_mesh_stats_t) */
    uint64_t total_requests;                /**< Total requests processed */
    uint64_t successful_convergences;       /**< Successful convergences */
    uint64_t failed_convergences;           /**< Failed convergences */
    uint64_t timeouts;                      /**< Timeout count */
    uint32_t active_participants;           /**< Active participants */

    /* Performance stats */
    float avg_latency_ms;                   /**< Average latency */
    float avg_iterations;                   /**< Average iterations to converge */
    float avg_agreement_score;              /**< Average agreement score */

    /* Algorithm stats */
    uint64_t algorithm_usage[LING_MESH_ALG_COUNT]; /**< Usage per algorithm */
    float avg_kl_divergence;                /**< Average KL divergence */
    float avg_lyapunov_value;               /**< Average Lyapunov value */

    /* Byzantine stats */
    uint64_t voting_fallbacks;              /**< Voting fallback count */
    uint64_t byzantine_detections;          /**< Byzantine nodes detected */
    uint64_t trimmed_beliefs;               /**< Beliefs trimmed */

    /* Health stats */
    uint64_t health_checks;                 /**< Health check count */
    uint64_t anomalies_detected;            /**< Anomalies detected */
    uint64_t heartbeat_misses;              /**< Missed heartbeats */
    float current_health_score;             /**< Current health score */

    /* Error stats */
    uint64_t exception_count;               /**< Exceptions thrown */
    uint64_t bbb_rejections;                /**< BBB validation rejections */
    uint64_t immune_notifications;          /**< Immune system notifications */
} linguistics_mesh_extended_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default mesh configuration
 *
 * @return Configuration with biologically-plausible defaults
 */
linguistics_mesh_config_t linguistics_mesh_default_config(void);

/**
 * @brief Get default algorithm configuration
 *
 * @return Algorithm configuration with optimal defaults
 */
linguistics_mesh_algorithm_config_t linguistics_mesh_default_algorithm_config(void);

/**
 * @brief Validate mesh configuration
 *
 * @param config Configuration to validate
 * @return true if valid
 */
bool linguistics_mesh_validate_config(const linguistics_mesh_config_t* config);

/**
 * @brief Create linguistics mesh coordinator
 *
 * Initializes the distributed mesh network for linguistics processing.
 * Sets up gossip beliefs, FEP orchestration, convergence detection,
 * and all infrastructure integrations (BBB, Health, KG, Exception).
 *
 * @param config Configuration (NULL for defaults)
 * @return Mesh handle or NULL on failure
 */
linguistics_mesh_t* linguistics_mesh_create(const linguistics_mesh_config_t* config);

/**
 * @brief Destroy linguistics mesh coordinator
 *
 * Cleans up all resources. Does NOT destroy registered participants.
 * Unregisters from health monitor and KG.
 *
 * @param mesh Mesh handle (NULL safe)
 */
void linguistics_mesh_destroy(linguistics_mesh_t* mesh);

/* ============================================================================
 * INFRASTRUCTURE INTEGRATION API
 * ============================================================================ */

/**
 * @brief Connect mesh to BBB system for input validation
 *
 * @param mesh Mesh coordinator
 * @param bbb BBB system handle
 * @return 0 on success, error code on failure
 */
int linguistics_mesh_connect_bbb(linguistics_mesh_t* mesh, bbb_system_t bbb);

/**
 * @brief Connect mesh to health monitor
 *
 * Registers the mesh with the health monitoring system for
 * real-time metrics collection and anomaly detection.
 *
 * @param mesh Mesh coordinator
 * @param health Health monitor handle (NULL to create internal)
 * @return 0 on success, error code on failure
 */
int linguistics_mesh_connect_health(linguistics_mesh_t* mesh, health_monitor_t health);

/**
 * @brief Connect mesh to brain KG for wiring
 *
 * Registers the mesh coordinator as a node in the brain KG
 * and wires edges to all registered participants.
 *
 * @param mesh Mesh coordinator
 * @param kg Brain KG handle
 * @return 0 on success, error code on failure
 */
int linguistics_mesh_connect_kg(linguistics_mesh_t* mesh, brain_kg_t* kg);

/**
 * @brief Start heartbeat monitoring
 *
 * Begins periodic heartbeat to indicate liveness.
 *
 * @param mesh Mesh coordinator
 * @return 0 on success, error code on failure
 */
int linguistics_mesh_start_heartbeat(linguistics_mesh_t* mesh);

/**
 * @brief Stop heartbeat monitoring
 *
 * @param mesh Mesh coordinator
 * @return 0 on success
 */
int linguistics_mesh_stop_heartbeat(linguistics_mesh_t* mesh);

/**
 * @brief Pulse heartbeat (manual call)
 *
 * Call this periodically if not using automatic heartbeat thread.
 *
 * @param mesh Mesh coordinator
 */
void linguistics_mesh_heartbeat_pulse(linguistics_mesh_t* mesh);

/**
 * @brief Get KG node ID for this mesh coordinator
 *
 * @param mesh Mesh coordinator
 * @return KG node ID, or BRAIN_KG_INVALID_NODE if not registered
 */
brain_kg_node_id_t linguistics_mesh_get_kg_node_id(const linguistics_mesh_t* mesh);

/* ============================================================================
 * PARTICIPANT REGISTRATION API
 * ============================================================================ */

/**
 * @brief Register a participant in the linguistics mesh
 *
 * Participants must implement the linguistics_mesh_handler_t interface:
 * - process(): Generate local belief from request
 * - update(): Update belief based on neighbor beliefs (FEP)
 * - get_precision(): Report current precision
 *
 * Also validates input via BBB and updates KG wiring.
 *
 * @param mesh Mesh coordinator
 * @param module_id Bio-async module ID
 * @param module_name Human-readable name
 * @param handler Handler interface with callbacks
 * @return 0 on success, error code on failure
 */
int linguistics_mesh_register_participant(
    linguistics_mesh_t* mesh,
    uint32_t module_id,
    const char* module_name,
    linguistics_mesh_handler_t handler
);

/**
 * @brief Unregister a participant from the mesh
 *
 * @param mesh Mesh coordinator
 * @param module_id Module to unregister
 * @return 0 on success, error code on failure
 */
int linguistics_mesh_unregister_participant(
    linguistics_mesh_t* mesh,
    uint32_t module_id
);

/**
 * @brief Set participant credibility (trust weight)
 *
 * Higher credibility = more influence in consensus.
 *
 * @param mesh Mesh coordinator
 * @param module_id Module to update
 * @param credibility New credibility [0,1]
 * @return 0 on success
 */
int linguistics_mesh_set_credibility(
    linguistics_mesh_t* mesh,
    uint32_t module_id,
    float credibility
);

/**
 * @brief Set mesh topology (neighbor connections)
 *
 * Defines the graph structure for algorithms that use topology.
 * If not set, defaults to fully connected mesh.
 *
 * @param mesh Mesh coordinator
 * @param adjacency_matrix NxN adjacency matrix (row-major, N = participant_count)
 * @return 0 on success
 */
int linguistics_mesh_set_topology(
    linguistics_mesh_t* mesh,
    const float* adjacency_matrix
);

/**
 * @brief Update PageRank credibility scores
 *
 * Runs PageRank algorithm to compute dynamic credibility
 * based on graph topology and past performance.
 *
 * @param mesh Mesh coordinator
 * @return 0 on success
 */
int linguistics_mesh_update_pagerank(linguistics_mesh_t* mesh);

/**
 * @brief Get participant by module ID
 *
 * @param mesh Mesh coordinator
 * @param module_id Module ID
 * @return Participant entry or NULL if not found
 */
const linguistics_mesh_participant_t* linguistics_mesh_get_participant(
    const linguistics_mesh_t* mesh,
    uint32_t module_id
);

/**
 * @brief Get number of active participants
 *
 * @param mesh Mesh coordinator
 * @return Count of active participants
 */
uint32_t linguistics_mesh_get_participant_count(const linguistics_mesh_t* mesh);

/* ============================================================================
 * REQUEST API
 * ============================================================================ */

/**
 * @brief Submit a linguistics request to the mesh
 *
 * This is the main entry point for distributed linguistics processing.
 * The mesh will:
 * 1. Validate input via BBB
 * 2. Broadcast request to all participants
 * 3. Collect initial beliefs with precision
 * 4. Run selected consensus algorithm
 * 5. Apply Byzantine tolerance if enabled
 * 6. Return precision-weighted consensus
 * 7. Record health metrics
 *
 * @param mesh Mesh coordinator
 * @param request Input request
 * @param response Output response (populated on return)
 * @param timeout_ms Timeout in milliseconds (0 = use config default)
 * @return 0 on success, error code on failure
 */
int linguistics_mesh_request(
    linguistics_mesh_t* mesh,
    const linguistics_request_t* request,
    linguistics_response_t* response,
    uint32_t timeout_ms
);

/**
 * @brief Submit async request (non-blocking)
 *
 * Returns immediately after broadcasting. Use linguistics_mesh_poll()
 * to check for completion.
 *
 * @param mesh Mesh coordinator
 * @param request Input request
 * @param request_id_out Output: request ID for polling
 * @return 0 on success
 */
int linguistics_mesh_request_async(
    linguistics_mesh_t* mesh,
    const linguistics_request_t* request,
    uint64_t* request_id_out
);

/**
 * @brief Poll async request status
 *
 * @param mesh Mesh coordinator
 * @param request_id Request ID from async call
 * @param response Output response (populated if complete)
 * @param state_out Output: current convergence state
 * @return 0 if complete, 1 if still processing, negative on error
 */
int linguistics_mesh_poll(
    linguistics_mesh_t* mesh,
    uint64_t request_id,
    linguistics_response_t* response,
    linguistics_convergence_state_t* state_out
);

/**
 * @brief Cancel an in-progress async request
 *
 * @param mesh Mesh coordinator
 * @param request_id Request ID to cancel
 * @return 0 on success
 */
int linguistics_mesh_cancel(
    linguistics_mesh_t* mesh,
    uint64_t request_id
);

/* ============================================================================
 * ALGORITHM API
 * ============================================================================ */

/**
 * @brief Set primary consensus algorithm
 *
 * @param mesh Mesh coordinator
 * @param algorithm Algorithm to use
 * @return 0 on success
 */
int linguistics_mesh_set_algorithm(
    linguistics_mesh_t* mesh,
    linguistics_mesh_algorithm_t algorithm
);

/**
 * @brief Set agreement metric
 *
 * @param mesh Mesh coordinator
 * @param metric Metric to use
 * @return 0 on success
 */
int linguistics_mesh_set_metric(
    linguistics_mesh_t* mesh,
    linguistics_mesh_metric_t metric
);

/**
 * @brief Run one iteration of belief propagation
 *
 * Sum-product algorithm for graphical model inference.
 *
 * @param mesh Mesh coordinator
 * @return Change in beliefs (L2 norm)
 */
float linguistics_mesh_bp_step(linguistics_mesh_t* mesh);

/**
 * @brief Run one iteration of FEP belief update
 *
 * Basic FEP: μ' = μ - lr × Π × ε
 *
 * @param mesh Mesh coordinator
 * @return Change in free energy (delta F)
 */
float linguistics_mesh_fep_step(linguistics_mesh_t* mesh);

/**
 * @brief Run one iteration of natural gradient descent
 *
 * θ_{t+1} = θ_t - η × F^{-1} × ∇L(θ)
 *
 * @param mesh Mesh coordinator
 * @return Change in beliefs
 */
float linguistics_mesh_natural_grad_step(linguistics_mesh_t* mesh);

/**
 * @brief Run one round of gossip propagation
 *
 * @param mesh Mesh coordinator
 * @return Number of beliefs propagated
 */
uint32_t linguistics_mesh_gossip_round(linguistics_mesh_t* mesh);

/**
 * @brief Run one iteration of graph Laplacian consensus
 *
 * ẋ = -L × x
 *
 * @param mesh Mesh coordinator
 * @return Change in beliefs
 */
float linguistics_mesh_laplacian_step(linguistics_mesh_t* mesh);

/**
 * @brief Compute covariance intersection fusion
 *
 * Optimal fusion when correlations are unknown.
 *
 * @param mesh Mesh coordinator
 * @param collective Output collective belief
 * @return 0 on success
 */
int linguistics_mesh_covariance_intersection(
    linguistics_mesh_t* mesh,
    linguistics_collective_belief_t* collective
);

/**
 * @brief Apply Byzantine tolerance filtering
 *
 * Removes outlier beliefs based on selected BFT mode.
 *
 * @param mesh Mesh coordinator
 * @return Number of beliefs excluded
 */
uint32_t linguistics_mesh_apply_bft(linguistics_mesh_t* mesh);

/**
 * @brief Compute Lyapunov function value
 *
 * V(x) = x^T × P × x for stability analysis.
 *
 * @param mesh Mesh coordinator
 * @return Lyapunov value (should decrease for convergence)
 */
float linguistics_mesh_compute_lyapunov(const linguistics_mesh_t* mesh);

/**
 * @brief Compute KL divergence between beliefs
 *
 * D_KL(P||Q) for information-theoretic agreement.
 *
 * @param mesh Mesh coordinator
 * @return Average pairwise KL divergence
 */
float linguistics_mesh_compute_kl_divergence(const linguistics_mesh_t* mesh);

/* ============================================================================
 * CONVERGENCE API
 * ============================================================================ */

/**
 * @brief Get current convergence state
 *
 * @param mesh Mesh coordinator
 * @param state Output state
 * @return 0 on success
 */
int linguistics_mesh_get_convergence_state(
    const linguistics_mesh_t* mesh,
    linguistics_convergence_state_t* state
);

/**
 * @brief Get collective belief for topic
 *
 * Returns precision-weighted consensus belief.
 *
 * @param mesh Mesh coordinator
 * @param topic Topic string
 * @param belief Output belief
 * @return 0 on success, error if topic not found
 */
int linguistics_mesh_get_collective_belief(
    const linguistics_mesh_t* mesh,
    const char* topic,
    linguistics_collective_belief_t* belief
);

/**
 * @brief Force voting fallback for current request
 *
 * Use when automatic deadlock detection doesn't trigger.
 *
 * @param mesh Mesh coordinator
 * @return 0 on success
 */
int linguistics_mesh_force_voting(linguistics_mesh_t* mesh);

/* ============================================================================
 * CONFIGURATION API
 * ============================================================================ */

/**
 * @brief Update mesh configuration at runtime
 *
 * @param mesh Mesh coordinator
 * @param config New configuration
 * @return 0 on success
 */
int linguistics_mesh_update_config(
    linguistics_mesh_t* mesh,
    const linguistics_mesh_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param mesh Mesh coordinator
 * @param config Output configuration
 * @return 0 on success
 */
int linguistics_mesh_get_config(
    const linguistics_mesh_t* mesh,
    linguistics_mesh_config_t* config
);

/**
 * @brief Set agreement threshold for convergence
 *
 * @param mesh Mesh coordinator
 * @param threshold New threshold [0,1]
 * @return 0 on success
 */
int linguistics_mesh_set_agreement_threshold(
    linguistics_mesh_t* mesh,
    float threshold
);

/**
 * @brief Set FEP learning rate
 *
 * @param mesh Mesh coordinator
 * @param learning_rate New learning rate (default: 0.1)
 * @return 0 on success
 */
int linguistics_mesh_set_learning_rate(
    linguistics_mesh_t* mesh,
    float learning_rate
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get mesh statistics
 *
 * @param mesh Mesh coordinator
 * @param stats Output statistics
 * @return 0 on success
 */
int linguistics_mesh_get_stats(
    const linguistics_mesh_t* mesh,
    linguistics_mesh_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param mesh Mesh coordinator
 */
void linguistics_mesh_reset_stats(linguistics_mesh_t* mesh);

/**
 * @brief Get average convergence latency
 *
 * @param mesh Mesh coordinator
 * @return Average latency in milliseconds
 */
float linguistics_mesh_get_avg_latency(const linguistics_mesh_t* mesh);

/**
 * @brief Get convergence success rate
 *
 * @param mesh Mesh coordinator
 * @return Success rate [0,1]
 */
float linguistics_mesh_get_success_rate(const linguistics_mesh_t* mesh);

/**
 * @brief Get current health score
 *
 * @param mesh Mesh coordinator
 * @return Health score [0-100]
 */
float linguistics_mesh_get_health_score(const linguistics_mesh_t* mesh);

/* ============================================================================
 * BIO-ASYNC INTEGRATION API
 * ============================================================================ */

/**
 * @brief Connect mesh to bio-async router
 *
 * Enables distributed messaging between participants.
 *
 * @param mesh Mesh coordinator
 * @return 0 on success
 */
int linguistics_mesh_connect_bio_async(linguistics_mesh_t* mesh);

/**
 * @brief Disconnect from bio-async
 *
 * @param mesh Mesh coordinator
 * @return 0 on success
 */
int linguistics_mesh_disconnect_bio_async(linguistics_mesh_t* mesh);

/**
 * @brief Process incoming bio-async message
 *
 * Call this when mesh receives a message from bio-async router.
 *
 * @param mesh Mesh coordinator
 * @param msg Message data
 * @param msg_len Message length
 * @return 0 on success
 */
int linguistics_mesh_process_message(
    linguistics_mesh_t* mesh,
    const void* msg,
    size_t msg_len
);

/* ============================================================================
 * DEBUG API
 * ============================================================================ */

/**
 * @brief Print mesh state for debugging
 *
 * @param mesh Mesh coordinator
 * @param verbose Include detailed participant info
 */
void linguistics_mesh_print_state(
    const linguistics_mesh_t* mesh,
    bool verbose
);

/**
 * @brief Get last error message
 *
 * @return Thread-local error string
 */
const char* linguistics_mesh_get_last_error(void);

/* ============================================================================
 * STRING CONVERSION
 * ============================================================================ */

/**
 * @brief Convert mesh state to string
 *
 * @param state Convergence state enum
 * @return Human-readable string
 */
const char* linguistics_mesh_state_to_string(mesh_convergence_state_t state);

/**
 * @brief Convert request type to string
 *
 * @param type Request type enum
 * @return Human-readable string
 */
const char* linguistics_request_type_to_string(linguistics_request_type_t type);

/**
 * @brief Convert algorithm to string
 *
 * @param algorithm Algorithm enum
 * @return Human-readable string
 */
const char* linguistics_mesh_algorithm_to_string(linguistics_mesh_algorithm_t algorithm);

/**
 * @brief Convert metric to string
 *
 * @param metric Metric enum
 * @return Human-readable string
 */
const char* linguistics_mesh_metric_to_string(linguistics_mesh_metric_t metric);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_LINGUISTICS_MESH_H */
