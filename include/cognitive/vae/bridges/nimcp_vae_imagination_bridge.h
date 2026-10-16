/**
 * @file nimcp_vae_imagination_bridge.h
 * @brief Bridge between VAE and Imagination Engine for Generative Simulation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integrates VAE latent generation with imagination/dreaming system
 *
 * WHY:  VAE provides ideal generative backbone for imagination:
 *       - Learned latent distribution for plausible sampling
 *       - Smooth interpolation for scene morphing
 *       - Variance as uncertainty/vividness control
 *       - Conditional generation for directed imagination
 *       - Quantum-enhanced sampling for constraint satisfaction
 *
 * HOW:  Bridge maps VAE capabilities to imagination modes:
 *       - PASSIVE: Free sampling from VAE prior
 *       - DIRECTED: Conditional VAE generation toward goals
 *       - COUNTERFACTUAL: Latent manipulation + decoding
 *       - PROSPECTIVE: Latent trajectory prediction
 *       - CREATIVE: High-temperature sampling (dream-like)
 *
 * QUANTUM ENHANCEMENTS:
 * =====================
 * - Quantum Monte Carlo: Enhanced sampling from multimodal latent distributions
 * - Quantum Walk: Exploration of latent space for diverse generation
 * - Quantum Annealing: Optimization toward goal states in imagination
 *
 * MATHEMATICAL UTILITIES:
 * =======================
 * - Hyperbolic Geometry: Hierarchical scene representation in Poincaré ball
 * - FFT: Spectral analysis for temporal imagination coherence
 * - Complex Math: Phase coherence for cross-modal binding
 *
 * BIO_MODULE: 0x1F15 (VAE-Imagination Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_IMAGINATION_BRIDGE_H
#define NIMCP_VAE_IMAGINATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bridge version */
#define VAE_IMAGINATION_BRIDGE_VERSION      "1.0.0"

/** Bio-async module ID */
#define BIO_MODULE_VAE_IMAGINATION_BRIDGE   0x1F15

/** Maximum scene elements */
#define VAE_IMAG_MAX_ELEMENTS               64

/** Maximum trajectory length */
#define VAE_IMAG_MAX_TRAJECTORY             256

/** Default sampling temperature for creative mode */
#define VAE_IMAG_CREATIVE_TEMPERATURE       1.5f

/** Default temperature for directed mode */
#define VAE_IMAG_DIRECTED_TEMPERATURE       0.7f

/** Quantum Monte Carlo default shots */
#define VAE_IMAG_QMC_DEFAULT_SHOTS          1000

/** Hyperbolic latent space curvature */
#define VAE_IMAG_HYPERBOLIC_CURVATURE       -1.0f

/** Error code range (32460-32469) */
#define NIMCP_ERROR_VAE_IMAG_BASE           32460
#define NIMCP_ERROR_VAE_IMAG_NULL           32461
#define NIMCP_ERROR_VAE_IMAG_NOT_CONNECTED  32462
#define NIMCP_ERROR_VAE_IMAG_SAMPLE_FAILED  32463
#define NIMCP_ERROR_VAE_IMAG_DECODE_FAILED  32464
#define NIMCP_ERROR_VAE_IMAG_NO_MEMORY      32465
#define NIMCP_ERROR_VAE_IMAG_GOAL_FAILED    32466
#define NIMCP_ERROR_VAE_IMAG_MODE_INVALID   32467
#define NIMCP_ERROR_VAE_IMAG_QUANTUM_FAILED 32468
#define NIMCP_ERROR_VAE_IMAG_COHERENCE_LOW  32469

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Imagination generation mode
 */
typedef enum {
    VAE_IMAG_MODE_PASSIVE = 0,       /**< Free dreaming from prior */
    VAE_IMAG_MODE_DIRECTED,           /**< Goal-directed generation */
    VAE_IMAG_MODE_COUNTERFACTUAL,     /**< What-if manipulation */
    VAE_IMAG_MODE_PROSPECTIVE,        /**< Future trajectory prediction */
    VAE_IMAG_MODE_RETROSPECTIVE,      /**< Memory reconstruction */
    VAE_IMAG_MODE_CREATIVE,           /**< High-temperature novel generation */
    VAE_IMAG_MODE_QUANTUM_SEARCH,     /**< Quantum-enhanced constraint search */
    VAE_IMAG_MODE_HYPERBOLIC          /**< Hierarchical scene generation */
} vae_imag_mode_t;

/**
 * @brief Sampling method for latent generation
 */
typedef enum {
    VAE_IMAG_SAMPLE_STANDARD = 0,    /**< Standard reparameterization */
    VAE_IMAG_SAMPLE_IMPORTANCE,       /**< Importance sampling */
    VAE_IMAG_SAMPLE_QMC,              /**< Quantum Monte Carlo enhanced */
    VAE_IMAG_SAMPLE_QUANTUM_WALK,     /**< Quantum walk exploration */
    VAE_IMAG_SAMPLE_ANNEALING         /**< Quantum annealing optimization */
} vae_imag_sample_method_t;

/**
 * @brief Latent space geometry
 */
typedef enum {
    VAE_IMAG_GEOMETRY_EUCLIDEAN = 0, /**< Standard Euclidean */
    VAE_IMAG_GEOMETRY_HYPERBOLIC,     /**< Poincaré ball hyperbolic */
    VAE_IMAG_GEOMETRY_SPHERICAL       /**< Spherical manifold */
} vae_imag_geometry_t;

/**
 * @brief Bridge state
 */
typedef enum {
    VAE_IMAG_STATE_DISCONNECTED = 0,
    VAE_IMAG_STATE_IDLE,
    VAE_IMAG_STATE_GENERATING,
    VAE_IMAG_STATE_MORPHING,
    VAE_IMAG_STATE_SEARCHING,
    VAE_IMAG_STATE_DREAMING,
    VAE_IMAG_STATE_ERROR
} vae_imag_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Sampling configuration
 */
typedef struct {
    vae_imag_sample_method_t method;  /**< Sampling method */
    float temperature;                 /**< Sampling temperature */
    float top_p;                       /**< Nucleus sampling threshold */
    uint32_t num_samples;              /**< Number of samples to generate */

    /* Quantum Monte Carlo parameters */
    uint32_t qmc_shots;               /**< QMC measurement shots */
    float qmc_target_acceptance;      /**< Target acceptance rate */
    uint32_t qmc_burnin;              /**< Burn-in samples */

    /* Quantum walk parameters */
    uint32_t qwalk_steps;             /**< Quantum walk steps */
    float qwalk_coin_bias;            /**< Coin operator bias */

    /* Annealing parameters */
    float annealing_initial_temp;     /**< Initial annealing temperature */
    float annealing_final_temp;       /**< Final temperature */
    uint32_t annealing_steps;         /**< Annealing schedule steps */
} vae_imag_sample_config_t;

/**
 * @brief Geometry configuration
 */
typedef struct {
    vae_imag_geometry_t geometry;     /**< Latent space geometry */
    float curvature;                   /**< Hyperbolic curvature (negative) */
    float boundary_clip;               /**< Poincaré boundary clipping */
    bool use_exponential_map;          /**< Use exp map for accuracy */
} vae_imag_geometry_config_t;

/**
 * @brief Goal specification for directed imagination
 */
typedef struct {
    float* target_features;           /**< Target latent features */
    uint32_t target_dim;              /**< Target dimension */
    float* constraints;               /**< Hard constraints */
    uint32_t constraint_dim;          /**< Constraint dimension */
    float* avoid_features;            /**< Features to avoid */
    uint32_t avoid_dim;               /**< Avoid dimension */
    float goal_weight;                /**< Weight for goal alignment */
    float constraint_strength;        /**< Constraint enforcement strength */
} vae_imag_goal_t;

/**
 * @brief Main bridge configuration
 */
typedef struct {
    vae_imag_sample_config_t sample;      /**< Sampling configuration */
    vae_imag_geometry_config_t geometry;  /**< Geometry configuration */

    /* Mode defaults */
    float passive_temperature;        /**< Temperature for passive mode */
    float directed_temperature;       /**< Temperature for directed mode */
    float creative_temperature;       /**< Temperature for creative mode */

    /* Quality settings */
    float vividness_target;           /**< Target vividness (0-1) */
    float coherence_threshold;        /**< Minimum coherence */
    float novelty_target;             /**< Target novelty level */

    /* Trajectory settings */
    uint32_t max_trajectory_length;   /**< Max trajectory steps */
    float trajectory_smoothing;       /**< Temporal smoothing factor */
    bool enable_trajectory_caching;   /**< Cache trajectory states */

    /* Integration options */
    bool enable_quantum_sampling;     /**< Use quantum-enhanced sampling */
    bool enable_hyperbolic_latent;    /**< Use hyperbolic latent space */
    bool enable_cross_modal;          /**< Cross-modal binding */
    bool enable_reality_checking;     /**< Reality/coherence checking */

    /* Logging */
    bool enable_logging;
    bool log_latent_samples;
} vae_imag_bridge_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Generation result from imagination
 */
typedef struct {
    float* generated;                 /**< Generated content */
    uint32_t generated_dim;           /**< Content dimension */
    float* latent;                    /**< Latent state used */
    uint32_t latent_dim;              /**< Latent dimension */

    /* Quality metrics */
    float vividness;                  /**< Achieved vividness */
    float coherence;                  /**< Achieved coherence */
    float novelty;                    /**< Novelty score */
    float goal_alignment;             /**< Alignment with goal (if directed) */
    float reality_distance;           /**< Distance from reality */

    /* Sampling info */
    vae_imag_sample_method_t method_used; /**< Actual method used */
    uint32_t samples_generated;       /**< Number of samples tried */
    float acceptance_rate;            /**< Acceptance rate (for MCMC) */

    /* Timing */
    uint64_t generation_time_us;      /**< Time to generate */
} vae_imag_generation_result_t;

/**
 * @brief Trajectory result for prospective/counterfactual
 */
typedef struct {
    float** trajectory;               /**< Array of latent states */
    float** decoded_trajectory;       /**< Decoded trajectory states */
    uint32_t trajectory_length;       /**< Actual length */
    uint32_t latent_dim;              /**< Latent dimension per step */
    uint32_t decoded_dim;             /**< Decoded dimension per step */
    float* coherence_scores;          /**< Per-step coherence */
    float avg_coherence;              /**< Average trajectory coherence */
    float endpoint_distance;          /**< Distance to goal (if any) */
} vae_imag_trajectory_result_t;

/**
 * @brief Quantum search result
 */
typedef struct {
    float* best_latent;               /**< Best latent state found */
    float* best_decoded;              /**< Best decoded output */
    uint32_t latent_dim;
    uint32_t decoded_dim;
    float constraint_satisfaction;    /**< How well constraints satisfied */
    float energy;                     /**< Final energy (for annealing) */
    uint32_t iterations;              /**< Iterations used */
    bool converged;                   /**< Whether search converged */
} vae_imag_quantum_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Operation counts */
    uint64_t total_generations;
    uint64_t passive_generations;
    uint64_t directed_generations;
    uint64_t creative_generations;
    uint64_t counterfactual_generations;
    uint64_t prospective_generations;
    uint64_t quantum_searches;

    /* Quality metrics */
    float avg_vividness;
    float avg_coherence;
    float avg_novelty;
    float avg_goal_alignment;
    float avg_acceptance_rate;

    /* Quantum statistics */
    uint64_t qmc_samples_total;
    uint64_t qwalk_steps_total;
    uint64_t annealing_runs;
    float avg_qmc_acceptance;

    /* Performance */
    float avg_generation_latency_us;
    float avg_trajectory_latency_us;
    float avg_search_latency_us;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_operation_us;
} vae_imag_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief VAE-Imagination bridge instance
 */
typedef struct vae_imag_bridge {
    vae_imag_bridge_config_t config;
    vae_system_t* vae;
    void* imagination_engine;         /**< imagination_engine_t* */
    vae_imag_bridge_state_t state;
    bool is_initialized;

    /* Dimension info */
    uint32_t vae_input_dim;
    uint32_t vae_latent_dim;
    uint32_t vae_output_dim;

    /* Working buffers */
    float* latent_buffer;
    float* decode_buffer;
    float* sample_buffer;

    /* Quantum state (if enabled) */
    void* qmc_state;                  /**< Quantum MC state */
    void* qwalk_state;                /**< Quantum walk state */

    /* Hyperbolic state (if enabled) */
    void* hyperbolic_space;           /**< Poincaré ball state */

    /* Statistics */
    vae_imag_bridge_stats_t stats;

    /* Current scenario tracking */
    uint64_t active_scenario_id;
    vae_imag_mode_t current_mode;

    /* Timing */
    uint64_t creation_time_us;
} vae_imag_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 */
int vae_imag_bridge_default_config(vae_imag_bridge_config_t* config);

/**
 * @brief Create VAE-Imagination bridge
 */
vae_imag_bridge_t* vae_imag_bridge_create(const vae_imag_bridge_config_t* config);

/**
 * @brief Destroy VAE-Imagination bridge
 */
void vae_imag_bridge_destroy(vae_imag_bridge_t* bridge);

/**
 * @brief Connect VAE to bridge
 */
int vae_imag_bridge_connect_vae(vae_imag_bridge_t* bridge, vae_system_t* vae);

/**
 * @brief Connect imagination engine to bridge
 */
int vae_imag_bridge_connect_imagination(vae_imag_bridge_t* bridge,
                                         void* imagination_engine);

/**
 * @brief Disconnect all systems
 */
int vae_imag_bridge_disconnect(vae_imag_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 */
bool vae_imag_bridge_is_connected(const vae_imag_bridge_t* bridge);

/* ============================================================================
 * Generation API
 * ============================================================================ */

/**
 * @brief Generate imagination content in specified mode
 *
 * @param bridge Bridge instance
 * @param mode Generation mode
 * @param goal Goal specification (NULL for undirected modes)
 * @param result Output generation result
 * @return 0 on success, error code on failure
 */
int vae_imag_generate(vae_imag_bridge_t* bridge,
                       vae_imag_mode_t mode,
                       const vae_imag_goal_t* goal,
                       vae_imag_generation_result_t* result);

/**
 * @brief Generate passive/dreaming content from prior
 */
int vae_imag_dream(vae_imag_bridge_t* bridge,
                    float temperature,
                    uint32_t num_samples,
                    vae_imag_generation_result_t* result);

/**
 * @brief Generate directed imagination toward goal
 */
int vae_imag_imagine_toward(vae_imag_bridge_t* bridge,
                             const vae_imag_goal_t* goal,
                             vae_imag_generation_result_t* result);

/**
 * @brief Generate creative/novel content (high temperature)
 */
int vae_imag_create(vae_imag_bridge_t* bridge,
                     float novelty_target,
                     vae_imag_generation_result_t* result);

/* ============================================================================
 * Trajectory Generation API
 * ============================================================================ */

/**
 * @brief Generate prospective trajectory (future simulation)
 *
 * @param bridge Bridge instance
 * @param start_state Starting latent state
 * @param start_dim Starting dimension
 * @param num_steps Number of steps to simulate
 * @param result Output trajectory result
 * @return 0 on success, error code on failure
 */
int vae_imag_simulate_future(vae_imag_bridge_t* bridge,
                              const float* start_state, uint32_t start_dim,
                              uint32_t num_steps,
                              vae_imag_trajectory_result_t* result);

/**
 * @brief Generate counterfactual trajectory
 */
int vae_imag_counterfactual(vae_imag_bridge_t* bridge,
                             const float* original_state, uint32_t state_dim,
                             const float* intervention, uint32_t intervention_dim,
                             uint32_t num_steps,
                             vae_imag_trajectory_result_t* result);

/**
 * @brief Interpolate between two imagination states
 */
int vae_imag_interpolate(vae_imag_bridge_t* bridge,
                          const float* state_a, const float* state_b,
                          uint32_t state_dim, uint32_t num_steps,
                          vae_imag_trajectory_result_t* result);

/* ============================================================================
 * Quantum-Enhanced Generation API
 * ============================================================================ */

/**
 * @brief Generate using quantum Monte Carlo sampling
 *
 * Uses QMC for more efficient exploration of multimodal latent distributions.
 *
 * @param bridge Bridge instance
 * @param target_features Target features to sample around
 * @param target_dim Target dimension
 * @param num_samples Number of QMC samples
 * @param result Output generation result
 * @return 0 on success, error code on failure
 */
int vae_imag_generate_qmc(vae_imag_bridge_t* bridge,
                           const float* target_features, uint32_t target_dim,
                           uint32_t num_samples,
                           vae_imag_generation_result_t* result);

/**
 * @brief Search latent space using quantum walk
 *
 * Quantum walk explores latent space for diverse, novel generations.
 */
int vae_imag_quantum_walk(vae_imag_bridge_t* bridge,
                           const float* start_state, uint32_t state_dim,
                           uint32_t num_steps,
                           vae_imag_generation_result_t* result);

/**
 * @brief Optimize toward goal using quantum annealing
 *
 * Finds latent state satisfying constraints via quantum annealing.
 */
int vae_imag_quantum_anneal(vae_imag_bridge_t* bridge,
                             const vae_imag_goal_t* goal,
                             vae_imag_quantum_result_t* result);

/**
 * @brief Constraint satisfaction search in latent space
 */
int vae_imag_quantum_search(vae_imag_bridge_t* bridge,
                             const float* constraints, uint32_t constraint_dim,
                             const float* weights, uint32_t num_constraints,
                             vae_imag_quantum_result_t* result);

/* ============================================================================
 * Hyperbolic Latent Space API
 * ============================================================================ */

/**
 * @brief Generate in hyperbolic latent space
 *
 * Uses Poincaré ball model for hierarchical scene generation.
 *
 * @param bridge Bridge instance
 * @param center_point Center in Poincaré ball
 * @param center_dim Center dimension
 * @param radius Hyperbolic radius for sampling
 * @param result Output generation result
 * @return 0 on success, error code on failure
 */
int vae_imag_hyperbolic_generate(vae_imag_bridge_t* bridge,
                                  const float* center_point, uint32_t center_dim,
                                  float radius,
                                  vae_imag_generation_result_t* result);

/**
 * @brief Compute hyperbolic distance between latent states
 */
int vae_imag_hyperbolic_distance(vae_imag_bridge_t* bridge,
                                  const float* state_a, const float* state_b,
                                  uint32_t dim, float* distance);

/**
 * @brief Hyperbolic geodesic interpolation
 */
int vae_imag_hyperbolic_geodesic(vae_imag_bridge_t* bridge,
                                  const float* state_a, const float* state_b,
                                  uint32_t dim, uint32_t num_steps,
                                  vae_imag_trajectory_result_t* result);

/**
 * @brief Map Euclidean latent to hyperbolic space
 */
int vae_imag_to_hyperbolic(vae_imag_bridge_t* bridge,
                            const float* euclidean, uint32_t dim,
                            float* hyperbolic);

/**
 * @brief Map hyperbolic back to Euclidean latent
 */
int vae_imag_from_hyperbolic(vae_imag_bridge_t* bridge,
                              const float* hyperbolic, uint32_t dim,
                              float* euclidean);

/* ============================================================================
 * Scene Manipulation API
 * ============================================================================ */

/**
 * @brief Inject element into current imagination
 */
int vae_imag_inject_element(vae_imag_bridge_t* bridge,
                             const float* element_features, uint32_t feature_dim,
                             float salience,
                             vae_imag_generation_result_t* result);

/**
 * @brief Remove element from current imagination
 */
int vae_imag_remove_element(vae_imag_bridge_t* bridge,
                             const float* element_features, uint32_t feature_dim,
                             vae_imag_generation_result_t* result);

/**
 * @brief Transform current imagination scene
 */
int vae_imag_transform(vae_imag_bridge_t* bridge,
                        const float* transformation, uint32_t transform_dim,
                        vae_imag_generation_result_t* result);

/**
 * @brief Blend two imagination scenes
 */
int vae_imag_blend(vae_imag_bridge_t* bridge,
                    const float* scene_a, const float* scene_b,
                    uint32_t dim, float blend_factor,
                    vae_imag_generation_result_t* result);

/* ============================================================================
 * Quality Evaluation API
 * ============================================================================ */

/**
 * @brief Evaluate imagination coherence
 */
int vae_imag_evaluate_coherence(vae_imag_bridge_t* bridge,
                                 const float* generated, uint32_t dim,
                                 float* coherence);

/**
 * @brief Evaluate novelty of generation
 */
int vae_imag_evaluate_novelty(vae_imag_bridge_t* bridge,
                               const float* generated, uint32_t dim,
                               float* novelty);

/**
 * @brief Evaluate reality distance
 */
int vae_imag_evaluate_reality(vae_imag_bridge_t* bridge,
                               const float* generated, uint32_t dim,
                               float* reality_distance);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge state
 */
vae_imag_bridge_state_t vae_imag_bridge_get_state(const vae_imag_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 */
int vae_imag_bridge_get_stats(const vae_imag_bridge_t* bridge,
                               vae_imag_bridge_stats_t* stats);

/**
 * @brief Get current mode
 */
vae_imag_mode_t vae_imag_get_current_mode(const vae_imag_bridge_t* bridge);

/* ============================================================================
 * Result Management
 * ============================================================================ */

/**
 * @brief Free generation result resources
 */
void vae_imag_generation_result_free(vae_imag_generation_result_t* result);

/**
 * @brief Free trajectory result resources
 */
void vae_imag_trajectory_result_free(vae_imag_trajectory_result_t* result);

/**
 * @brief Free quantum result resources
 */
void vae_imag_quantum_result_free(vae_imag_quantum_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_IMAGINATION_BRIDGE_H */
