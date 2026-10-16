/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_infogeo_snn_bridge.h - Information Geometry to SNN Integration Bridge
//=============================================================================
/**
 * @file nimcp_infogeo_snn_bridge.h
 * @brief Bridge connecting Information Geometry with Spiking Neural Networks
 *
 * WHAT: Provides bidirectional integration between Information Geometry module
 *       and SNN systems for geometry-aware neural network optimization.
 *
 * WHY:  Information geometry enables superior SNN learning:
 *       - Natural gradient descent converges 2-10x faster than standard SGD
 *       - Fisher information captures parameter sensitivity in SNN dynamics
 *       - Neural manifold structure reveals low-dimensional SNN representations
 *       - Geodesic distances provide principled similarity measures
 *
 * HOW:  Two-way integration:
 *       1. InfoGeo -> SNN: Natural gradient updates for synaptic weights
 *       2. SNN -> InfoGeo: Spike statistics define probability distributions
 *       3. Fisher matrix computed from spike train likelihoods
 *       4. Manifold structure estimated from population activity
 *
 * BIOLOGICAL BASIS:
 * ```
 * INFORMATION GEOMETRY                    SNN APPLICATION
 * -----------------------------------------------------------------------
 * Fisher Information Matrix           ->  Measures curvature of spike likelihood
 * Natural Gradient                    ->  Optimal weight updates preserving info
 * Riemannian Manifold                 ->  Low-D structure of neural activity
 * KL Divergence                       ->  Distance between firing patterns
 * Geodesic Distance                   ->  Shortest path in probability space
 * Ricci Curvature                     ->  Neural manifold complexity
 * ```
 *
 * KEY BENEFITS:
 * - Parameter-invariant learning (independent of parameterization)
 * - Faster convergence through second-order geometry
 * - Better generalization via manifold regularization
 * - Principled distance metrics for spike train similarity
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_INFOGEO_SNN_BRIDGE_H
#define NIMCP_INFOGEO_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define INFOGEO_SNN_MODULE_NAME         "infogeo_snn_bridge"

/** Maximum neurons for Fisher computation */
#define INFOGEO_SNN_MAX_NEURONS         1024

/** Maximum synapses per batch update */
#define INFOGEO_SNN_MAX_SYNAPSES        4096

/** Default Fisher regularization */
#define INFOGEO_SNN_FISHER_REG          1e-5f

/** Default natural gradient learning rate */
#define INFOGEO_SNN_NAT_GRAD_LR         0.01f

/** Maximum spike window for statistics (ms) */
#define INFOGEO_SNN_MAX_SPIKE_WINDOW    100.0f

/** Fisher update frequency (every N batches) */
#define INFOGEO_SNN_FISHER_UPDATE_FREQ  10

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Natural gradient method for SNN
 */
typedef enum {
    INFOGEO_SNN_NATGRAD_EXACT = 0,      /**< Exact Fisher inverse (small networks) */
    INFOGEO_SNN_NATGRAD_DIAGONAL,       /**< Diagonal Fisher approximation */
    INFOGEO_SNN_NATGRAD_KFAC,           /**< Kronecker-factored approximate curvature */
    INFOGEO_SNN_NATGRAD_EMPIRICAL       /**< Empirical Fisher from samples */
} infogeo_snn_natgrad_method_t;

/**
 * @brief Spike probability model
 */
typedef enum {
    INFOGEO_SNN_PROB_POISSON = 0,       /**< Poisson spike model */
    INFOGEO_SNN_PROB_BERNOULLI,         /**< Bernoulli spike model */
    INFOGEO_SNN_PROB_GLM,               /**< Generalized linear model */
    INFOGEO_SNN_PROB_EXPONENTIAL        /**< Exponential family model */
} infogeo_snn_prob_model_t;

/**
 * @brief Manifold estimation method
 */
typedef enum {
    INFOGEO_SNN_MANIFOLD_PCA = 0,       /**< Principal component analysis */
    INFOGEO_SNN_MANIFOLD_ISOMAP,        /**< Isometric mapping */
    INFOGEO_SNN_MANIFOLD_LLE,           /**< Locally linear embedding */
    INFOGEO_SNN_MANIFOLD_UMAP           /**< UMAP embedding */
} infogeo_snn_manifold_method_t;

/**
 * @brief Bridge operation mode
 */
typedef enum {
    INFOGEO_SNN_MODE_TRAINING = 0,      /**< Training mode (compute Fisher) */
    INFOGEO_SNN_MODE_INFERENCE,         /**< Inference mode (fixed geometry) */
    INFOGEO_SNN_MODE_ANALYSIS           /**< Analysis mode (manifold study) */
} infogeo_snn_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for Information Geometry-SNN bridge
 */
typedef struct {
    /** Natural gradient settings */
    infogeo_snn_natgrad_method_t natgrad_method;  /**< Natural gradient method */
    float natural_learning_rate;                   /**< Natural gradient LR */
    float fisher_regularization;                   /**< Regularization for inversion */
    uint32_t fisher_update_frequency;              /**< Batches between Fisher updates */
    bool enable_fisher_damping;                    /**< Adaptive damping */
    float damping_factor;                          /**< Initial damping */

    /** Spike probability model */
    infogeo_snn_prob_model_t prob_model;          /**< Probability model for spikes */
    float spike_bin_width_ms;                      /**< Time bin for spike discretization */
    uint32_t spike_history_length;                 /**< Spike history for GLM */

    /** Manifold settings */
    infogeo_snn_manifold_method_t manifold_method; /**< Manifold estimation method */
    uint32_t latent_dim;                           /**< Target latent dimensionality */
    float neighborhood_radius;                      /**< Radius for local geometry */
    bool compute_curvature;                        /**< Compute Ricci curvature */

    /** Operation settings */
    infogeo_snn_mode_t mode;                       /**< Operation mode */
    float update_interval_ms;                      /**< Bridge update interval */
    bool enable_ema;                               /**< EMA for Fisher smoothing */
    float ema_decay;                               /**< EMA decay rate */

    /** Feature flags */
    bool enable_manifold_projection;               /**< Project weights to manifold */
    bool enable_geodesic_interpolation;            /**< Geodesic weight interpolation */
    bool enable_kl_regularization;                 /**< KL divergence regularization */
    bool enable_logging;                           /**< Enable logging */
} infogeo_snn_config_t;

/**
 * @brief Spike train data for geometry computation
 */
typedef struct {
    uint32_t neuron_id;                 /**< Neuron identifier */
    float* spike_times;                 /**< Array of spike times (ms) */
    uint32_t num_spikes;                /**< Number of spikes */
    float window_start_ms;              /**< Window start time */
    float window_end_ms;                /**< Window end time */
    float firing_rate;                  /**< Computed firing rate (Hz) */
} infogeo_snn_spikes_t;

/**
 * @brief Natural gradient update result
 */
typedef struct {
    float* natural_gradient;            /**< Computed natural gradient */
    uint32_t gradient_size;             /**< Size of gradient vector */
    float gradient_norm;                /**< Standard gradient norm */
    float natural_grad_norm;            /**< Natural gradient norm */
    float speedup_ratio;                /**< Natural/standard ratio */
    float fisher_condition_number;      /**< Fisher matrix condition number */
    bool gradient_clipped;              /**< Whether gradient was clipped */
} infogeo_snn_update_t;

/**
 * @brief Neural manifold state
 */
typedef struct {
    float* embedding;                   /**< Current manifold embedding */
    uint32_t embedding_dim;             /**< Embedding dimensionality */
    float* principal_directions;        /**< Principal directions matrix */
    float explained_variance;           /**< Variance explained by embedding */
    float intrinsic_dim_estimate;       /**< Estimated intrinsic dimension */
    float ricci_curvature;              /**< Local Ricci curvature */
    float geodesic_distance;            /**< Distance to reference point */
} infogeo_snn_manifold_t;

/**
 * @brief KL divergence between spike distributions
 */
typedef struct {
    float kl_forward;                   /**< KL(P||Q) */
    float kl_reverse;                   /**< KL(Q||P) */
    float kl_symmetric;                 /**< (KL(P||Q) + KL(Q||P))/2 */
    float js_divergence;                /**< Jensen-Shannon divergence */
    uint32_t sample_size;               /**< Samples used for computation */
} infogeo_snn_divergence_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t natgrad_updates;           /**< Natural gradient updates */
    uint64_t fisher_computations;       /**< Fisher matrix computations */
    uint64_t manifold_projections;      /**< Manifold projections */
    uint64_t kl_computations;           /**< KL divergence computations */
    float avg_speedup_ratio;            /**< Average natural/standard speedup */
    float avg_fisher_condition;         /**< Average Fisher condition number */
    float avg_convergence_rate;         /**< Average convergence rate */
    float total_weight_update;          /**< Total weight change magnitude */
    float last_update_ms;               /**< Last update timestamp */
} infogeo_snn_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct infogeo_snn_bridge_struct infogeo_snn_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_default_config(infogeo_snn_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Information Geometry-SNN bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT infogeo_snn_bridge_t* infogeo_snn_bridge_create(
    const infogeo_snn_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void infogeo_snn_bridge_destroy(infogeo_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_reset(infogeo_snn_bridge_t* bridge);

//=============================================================================
// Natural Gradient API (InfoGeo -> SNN)
//=============================================================================

/**
 * @brief Register spike data for Fisher computation
 *
 * WHAT: Adds spike train data for Fisher information estimation
 * WHY:  Fisher matrix requires spike likelihood gradients
 * HOW:  Accumulates spike statistics for empirical Fisher
 *
 * @param bridge Bridge handle
 * @param spikes Spike train data
 * @param num_neurons Number of neurons
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_register_spikes(
    infogeo_snn_bridge_t* bridge,
    const infogeo_snn_spikes_t* spikes,
    uint32_t num_neurons
);

/**
 * @brief Compute Fisher information matrix
 *
 * WHAT: Computes Fisher matrix from registered spike data
 * WHY:  Fisher matrix defines Riemannian metric on parameter space
 * HOW:  Uses configured probability model and estimation method
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_compute_fisher(infogeo_snn_bridge_t* bridge);

/**
 * @brief Compute natural gradient from standard gradient
 *
 * WHAT: Transforms standard gradient to natural gradient
 * WHY:  Natural gradient is optimal for KL divergence minimization
 * HOW:  Multiplies gradient by inverse Fisher matrix
 *
 * @param bridge Bridge handle
 * @param gradient Standard gradient vector
 * @param grad_size Size of gradient vector
 * @param result Output natural gradient result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_natural_gradient(
    infogeo_snn_bridge_t* bridge,
    const float* gradient,
    uint32_t grad_size,
    infogeo_snn_update_t* result
);

/**
 * @brief Apply natural gradient update to weights
 *
 * WHAT: Updates SNN weights using natural gradient
 * WHY:  Single-call convenience for weight update
 * HOW:  Computes natural gradient and applies with learning rate
 *
 * @param bridge Bridge handle
 * @param weights Weight vector to update (modified in place)
 * @param gradient Standard gradient
 * @param size Vector size
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_update_weights(
    infogeo_snn_bridge_t* bridge,
    float* weights,
    const float* gradient,
    uint32_t size
);

//=============================================================================
// Manifold API
//=============================================================================

/**
 * @brief Add population activity sample for manifold estimation
 *
 * WHAT: Adds neural activity sample to manifold dataset
 * WHY:  Manifold structure requires multiple activity samples
 * HOW:  Stores activity vector for dimensionality reduction
 *
 * @param bridge Bridge handle
 * @param activity Neural activity vector (firing rates)
 * @param activity_size Size of activity vector
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_add_activity_sample(
    infogeo_snn_bridge_t* bridge,
    const float* activity,
    uint32_t activity_size
);

/**
 * @brief Estimate neural manifold structure
 *
 * WHAT: Computes manifold embedding and geometry
 * WHY:  Reveals low-dimensional structure of neural activity
 * HOW:  Uses configured dimensionality reduction method
 *
 * @param bridge Bridge handle
 * @param manifold Output manifold state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_estimate_manifold(
    infogeo_snn_bridge_t* bridge,
    infogeo_snn_manifold_t* manifold
);

/**
 * @brief Project activity onto manifold
 *
 * WHAT: Projects new activity onto learned manifold
 * WHY:  Enables manifold-aware processing of new data
 * HOW:  Uses manifold projection mapping
 *
 * @param bridge Bridge handle
 * @param activity Neural activity vector
 * @param activity_size Size of activity vector
 * @param projected Output projected coordinates
 * @param projected_size Size of projected vector
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_project_activity(
    infogeo_snn_bridge_t* bridge,
    const float* activity,
    uint32_t activity_size,
    float* projected,
    uint32_t projected_size
);

/**
 * @brief Compute geodesic distance on manifold
 *
 * WHAT: Computes geodesic distance between two activity patterns
 * WHY:  Geodesic is principled distance on curved manifold
 * HOW:  Finds shortest path on Riemannian manifold
 *
 * @param bridge Bridge handle
 * @param activity_a First activity pattern
 * @param activity_b Second activity pattern
 * @param size Activity vector size
 * @param distance Output geodesic distance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_geodesic_distance(
    infogeo_snn_bridge_t* bridge,
    const float* activity_a,
    const float* activity_b,
    uint32_t size,
    float* distance
);

//=============================================================================
// Divergence API
//=============================================================================

/**
 * @brief Compute KL divergence between spike distributions
 *
 * WHAT: Computes KL divergence between two spike patterns
 * WHY:  KL divergence measures information loss between distributions
 * HOW:  Uses configured probability model for spike likelihoods
 *
 * @param bridge Bridge handle
 * @param spikes_p First spike pattern (P distribution)
 * @param spikes_q Second spike pattern (Q distribution)
 * @param num_neurons Number of neurons
 * @param divergence Output divergence metrics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_kl_divergence(
    infogeo_snn_bridge_t* bridge,
    const infogeo_snn_spikes_t* spikes_p,
    const infogeo_snn_spikes_t* spikes_q,
    uint32_t num_neurons,
    infogeo_snn_divergence_t* divergence
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Maintains Fisher EMA, updates manifold estimates
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_update(
    infogeo_snn_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_get_stats(
    const infogeo_snn_bridge_t* bridge,
    infogeo_snn_stats_t* stats
);

/**
 * @brief Get current manifold state
 *
 * @param bridge Bridge handle
 * @param manifold Output manifold state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_get_manifold(
    const infogeo_snn_bridge_t* bridge,
    infogeo_snn_manifold_t* manifold
);

/**
 * @brief Get Fisher matrix condition number
 *
 * @param bridge Bridge handle
 * @return Condition number, or -1.0 on error
 */
NIMCP_EXPORT float infogeo_snn_get_fisher_condition(
    const infogeo_snn_bridge_t* bridge
);

/**
 * @brief Check if Fisher matrix is well-conditioned
 *
 * @param bridge Bridge handle
 * @return true if Fisher is usable for inversion
 */
NIMCP_EXPORT bool infogeo_snn_fisher_valid(
    const infogeo_snn_bridge_t* bridge
);

/**
 * @brief Get current mode
 *
 * @param bridge Bridge handle
 * @return Current operation mode
 */
NIMCP_EXPORT infogeo_snn_mode_t infogeo_snn_get_mode(
    const infogeo_snn_bridge_t* bridge
);

/**
 * @brief Set operation mode
 *
 * @param bridge Bridge handle
 * @param mode New operation mode
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_snn_set_mode(
    infogeo_snn_bridge_t* bridge,
    infogeo_snn_mode_t mode
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFOGEO_SNN_BRIDGE_H */