/**
 * @file nimcp_information_geometry.h
 * @brief Information Geometry Module - Learning optimization through Fisher metrics
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Information geometry for neural network learning optimization
 * WHY:  Natural gradient descent converges 2-10x faster than standard SGD
 * HOW:  Fisher information matrix defines metric on parameter manifold
 *
 * KEY CONCEPTS:
 * - Fisher Information Matrix: Measures curvature of log-likelihood
 * - Natural Gradient: Gradient adjusted by inverse Fisher matrix
 * - Neural Manifold: Low-dimensional structure of neural activity
 * - Ricci Curvature: Measures how volume changes along geodesics
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_INFORMATION_GEOMETRY_H
#define NIMCP_INFORMATION_GEOMETRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum latent dimensions for manifold */
#define INFO_GEOM_MAX_LATENT_DIM        64

/** Maximum ambient dimensions */
#define INFO_GEOM_MAX_AMBIENT_DIM       1024

/** Default latent dimension */
#define INFO_GEOM_DEFAULT_LATENT_DIM    16

/** Regularization for matrix inversion */
#define INFO_GEOM_REGULARIZATION        1e-6f

/** Natural gradient clipping threshold */
#define INFO_GEOM_GRAD_CLIP             10.0f

//=============================================================================
// Error Codes
//=============================================================================

typedef enum {
    INFO_GEOM_OK = 0,
    INFO_GEOM_ERR_NULL_PTR = -1,
    INFO_GEOM_ERR_INVALID_DIM = -2,
    INFO_GEOM_ERR_SINGULAR_MATRIX = -3,
    INFO_GEOM_ERR_NOT_INITIALIZED = -4,
    INFO_GEOM_ERR_ALREADY_INITIALIZED = -5,
    INFO_GEOM_ERR_NO_MEMORY = -6,
    INFO_GEOM_ERR_COMPUTATION = -7
} nimcp_info_geom_error_t;

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_brain_struct* nimcp_brain_t;
typedef struct nimcp_bio_router_struct* nimcp_bio_router_t;

//=============================================================================
// Opaque Handles
//=============================================================================

typedef struct nimcp_info_geometry_struct* nimcp_info_geometry_t;
typedef struct nimcp_fisher_info_struct* nimcp_fisher_info_t;
typedef struct nimcp_natural_gradient_struct* nimcp_natural_gradient_t;
typedef struct nimcp_neural_manifold_struct* nimcp_neural_manifold_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Information geometry configuration
 */
typedef struct {
    uint32_t latent_dim;            /**< Dimension of latent space */
    uint32_t ambient_dim;           /**< Dimension of ambient space */
    float regularization;           /**< Regularization for matrix inversion */
    float learning_rate;            /**< Base learning rate */
    float gradient_clip;            /**< Gradient clipping threshold */
    bool enable_ema;                /**< Enable exponential moving average */
    float ema_decay;                /**< EMA decay rate (0.9-0.999) */
    bool enable_logging;            /**< Enable logging */
    bool enable_metrics;            /**< Enable metrics collection */
} nimcp_info_geom_config_t;

/**
 * @brief Fisher information configuration
 */
typedef struct {
    uint32_t param_dim;             /**< Number of parameters */
    uint32_t sample_size;           /**< Samples for empirical Fisher */
    float regularization;           /**< Diagonal regularization */
    bool use_empirical;             /**< Use empirical vs exact Fisher */
    bool enable_damping;            /**< Enable adaptive damping */
    float initial_damping;          /**< Initial damping factor */
} nimcp_fisher_config_t;

/**
 * @brief Natural gradient configuration
 */
typedef struct {
    float learning_rate;            /**< Natural gradient learning rate */
    float momentum;                 /**< Momentum coefficient */
    float gradient_clip;            /**< Clip threshold */
    bool use_preconditioner;        /**< Use Fisher as preconditioner */
    bool enable_warmup;             /**< Enable learning rate warmup */
    uint32_t warmup_steps;          /**< Warmup steps */
} nimcp_natural_grad_config_t;

/**
 * @brief Neural manifold configuration
 */
typedef struct {
    uint32_t intrinsic_dim;         /**< Estimated intrinsic dimensionality */
    uint32_t num_samples;           /**< Samples for manifold estimation */
    float neighborhood_radius;      /**< Radius for local geometry */
    bool compute_curvature;         /**< Compute Ricci curvature */
    bool enable_embedding;          /**< Enable manifold embedding */
} nimcp_manifold_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Information geometry state
 */
typedef struct {
    float embedding[INFO_GEOM_MAX_LATENT_DIM];
    float ricci_curvature;
    float geodesic_distance;
    float kl_divergence;
    bool is_initialized;
    uint64_t update_count;
} nimcp_info_geom_state_t;

/**
 * @brief Information geometry statistics
 */
typedef struct {
    float avg_curvature;
    float avg_gradient_norm;
    float avg_natural_grad_norm;
    float avg_speedup_ratio;        /**< Natural/standard gradient ratio */
    uint64_t updates;
    uint64_t fisher_computations;
    float convergence_rate;
} nimcp_info_geom_stats_t;

//=============================================================================
// Information Geometry API
//=============================================================================

/**
 * @brief Get default configuration
 */
NIMCP_EXPORT nimcp_info_geom_config_t nimcp_info_geom_default_config(void);

/**
 * @brief Create information geometry system
 */
NIMCP_EXPORT nimcp_info_geometry_t nimcp_info_geom_create(
    const nimcp_info_geom_config_t* config
);

/**
 * @brief Destroy information geometry system
 */
NIMCP_EXPORT void nimcp_info_geom_destroy(nimcp_info_geometry_t geom);

/**
 * @brief Initialize with brain connection
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_info_geom_init(
    nimcp_info_geometry_t geom,
    nimcp_brain_t brain
);

/**
 * @brief Shutdown information geometry
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_info_geom_shutdown(
    nimcp_info_geometry_t geom
);

/**
 * @brief Compute Fisher information matrix from distribution
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_info_geom_compute_fisher(
    nimcp_info_geometry_t geom,
    const float* distribution,
    uint32_t dist_size
);

/**
 * @brief Compute natural gradient from standard gradient
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_info_geom_natural_gradient(
    nimcp_info_geometry_t geom,
    const float* gradient,
    float* natural_grad,
    uint32_t grad_size
);

/**
 * @brief Update parameters using natural gradient
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_info_geom_update(
    nimcp_info_geometry_t geom,
    float* parameters,
    const float* gradient,
    uint32_t param_size,
    float learning_rate
);

/**
 * @brief Compute geodesic distance between two points
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_info_geom_geodesic_distance(
    nimcp_info_geometry_t geom,
    const float* point_a,
    const float* point_b,
    uint32_t dim,
    float* distance
);

/**
 * @brief Compute KL divergence between distributions
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_info_geom_kl_divergence(
    nimcp_info_geometry_t geom,
    const float* p,
    const float* q,
    uint32_t size,
    float* kl_div
);

/**
 * @brief Get current state
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_info_geom_get_state(
    nimcp_info_geometry_t geom,
    nimcp_info_geom_state_t* state
);

/**
 * @brief Get statistics
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_info_geom_get_stats(
    nimcp_info_geometry_t geom,
    nimcp_info_geom_stats_t* stats
);

/**
 * @brief Reset statistics
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_info_geom_reset_stats(
    nimcp_info_geometry_t geom
);

//=============================================================================
// Fisher Information API
//=============================================================================

/**
 * @brief Get default Fisher config
 */
NIMCP_EXPORT nimcp_fisher_config_t nimcp_fisher_default_config(void);

/**
 * @brief Create Fisher information computer
 */
NIMCP_EXPORT nimcp_fisher_info_t nimcp_fisher_create(
    const nimcp_fisher_config_t* config
);

/**
 * @brief Destroy Fisher computer
 */
NIMCP_EXPORT void nimcp_fisher_destroy(nimcp_fisher_info_t fisher);

/**
 * @brief Compute Fisher matrix from gradients
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_fisher_compute(
    nimcp_fisher_info_t fisher,
    const float* gradients,
    uint32_t num_samples,
    uint32_t grad_dim
);

/**
 * @brief Compute empirical Fisher from samples
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_fisher_compute_empirical(
    nimcp_fisher_info_t fisher,
    const float* samples,
    const float* log_probs,
    uint32_t num_samples,
    uint32_t sample_dim
);

/**
 * @brief Get Fisher matrix (or its inverse)
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_fisher_get_matrix(
    nimcp_fisher_info_t fisher,
    float* matrix,
    uint32_t size,
    bool get_inverse
);

/**
 * @brief Apply Fisher inverse to vector (solve F*x = b)
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_fisher_solve(
    nimcp_fisher_info_t fisher,
    const float* b,
    float* x,
    uint32_t size
);

//=============================================================================
// Natural Gradient API
//=============================================================================

/**
 * @brief Get default natural gradient config
 */
NIMCP_EXPORT nimcp_natural_grad_config_t nimcp_natural_grad_default_config(void);

/**
 * @brief Create natural gradient optimizer
 */
NIMCP_EXPORT nimcp_natural_gradient_t nimcp_natural_grad_create(
    const nimcp_natural_grad_config_t* config,
    uint32_t param_dim
);

/**
 * @brief Destroy natural gradient optimizer
 */
NIMCP_EXPORT void nimcp_natural_grad_destroy(nimcp_natural_gradient_t ng);

/**
 * @brief Update Fisher information for preconditioner
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_natural_grad_update_fisher(
    nimcp_natural_gradient_t ng,
    nimcp_fisher_info_t fisher
);

/**
 * @brief Compute natural gradient step
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_natural_grad_step(
    nimcp_natural_gradient_t ng,
    float* parameters,
    const float* gradient,
    uint32_t size
);

/**
 * @brief Get effective learning rate
 */
NIMCP_EXPORT float nimcp_natural_grad_get_lr(nimcp_natural_gradient_t ng);

//=============================================================================
// Neural Manifold API
//=============================================================================

/**
 * @brief Get default manifold config
 */
NIMCP_EXPORT nimcp_manifold_config_t nimcp_manifold_default_config(void);

/**
 * @brief Create neural manifold analyzer
 */
NIMCP_EXPORT nimcp_neural_manifold_t nimcp_manifold_create(
    const nimcp_manifold_config_t* config,
    uint32_t ambient_dim
);

/**
 * @brief Destroy manifold analyzer
 */
NIMCP_EXPORT void nimcp_manifold_destroy(nimcp_neural_manifold_t manifold);

/**
 * @brief Add samples to manifold
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_manifold_add_samples(
    nimcp_neural_manifold_t manifold,
    const float* samples,
    uint32_t num_samples,
    uint32_t sample_dim
);

/**
 * @brief Estimate intrinsic dimensionality
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_manifold_estimate_dim(
    nimcp_neural_manifold_t manifold,
    uint32_t* intrinsic_dim
);

/**
 * @brief Compute local curvature at point
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_manifold_curvature(
    nimcp_neural_manifold_t manifold,
    const float* point,
    uint32_t dim,
    float* curvature
);

/**
 * @brief Project point onto manifold
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_manifold_project(
    nimcp_neural_manifold_t manifold,
    const float* point,
    float* projected,
    uint32_t dim
);

/**
 * @brief Compute geodesic between points on manifold
 */
NIMCP_EXPORT nimcp_info_geom_error_t nimcp_manifold_geodesic(
    nimcp_neural_manifold_t manifold,
    const float* start,
    const float* end,
    float* path,
    uint32_t path_steps,
    uint32_t dim
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string
 */
NIMCP_EXPORT const char* nimcp_info_geom_error_string(nimcp_info_geom_error_t err);

//=============================================================================
// Wave W15: Runtime manifold-tracking events (aggregated)
//
// Emit only when the natural-gradient speedup ratio or the Fisher
// conditioning crosses a threshold, or on EMA-warmup completion.
// Never per-sample / per-Fisher update.
//=============================================================================

#ifndef NIMCP_BRAIN_T_DEFINED
#define NIMCP_BRAIN_T_DEFINED
typedef struct brain_struct* brain_t;
#endif

struct brain_kg;

/**
 * @brief Register brain handle for info-geom runtime KG emission.
 */
NIMCP_EXPORT void nimcp_info_geom_kg_register_brain(brain_t brain);

/**
 * @brief Emit an aggregated info-geom manifold-tracking event.
 *
 * @param brain             Brain handle (falls back to registered)
 * @param kind              "speedup_shift", "fisher_singular",
 *                          "embedding_converged", "curvature_spike"
 * @param speedup_ratio     Natural/standard gradient norm ratio
 * @param ricci_curvature   Current Ricci curvature (NaN to skip)
 * @param ts_us             Timestamp in microseconds
 */
NIMCP_EXPORT void nimcp_info_geom_kg_trigger_manifold_event(
    brain_t brain,
    const char* kind,
    float speedup_ratio,
    float ricci_curvature,
    uint64_t ts_us
);

/**
 * @brief Read-path: count info-geom manifold events.
 */
NIMCP_EXPORT uint32_t nimcp_info_geom_kg_count_events(
    struct brain_kg* kg,
    const char* kind_substr
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFORMATION_GEOMETRY_H */
