/**
 * @file nimcp_vae_loss.h
 * @brief VAE Loss Computation - ELBO and FEP-compatible free energy
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Computes VAE loss functions including reconstruction loss,
 *       KL divergence, and FEP-compatible variational free energy.
 *
 * WHY:  The VAE objective (ELBO) directly maps to variational free energy:
 *       - Reconstruction loss = -E[log p(x|z)] = prediction error
 *       - KL divergence = complexity cost = model complexity penalty
 *       This provides a principled foundation for FEP integration.
 *
 * HOW:  - Multiple reconstruction loss types (MSE, BCE, Gaussian NLL)
 *       - β-VAE weighting for disentanglement
 *       - Free bits to prevent posterior collapse
 *       - Precision-weighted loss for FEP compatibility
 *
 * ELBO DECOMPOSITION:
 * ```
 *   ELBO = E_q[log p(x|z)] - KL[q(z|x) || p(z)]
 *        = -Reconstruction_Loss - KL_Divergence
 *
 *   Variational Free Energy F = -ELBO
 *                             = Reconstruction_Loss + KL_Divergence
 *                             = Prediction_Error + Complexity_Cost
 * ```
 *
 * β-VAE OBJECTIVE:
 * ```
 *   L_β = E_q[log p(x|z)] - β * KL[q(z|x) || p(z)]
 *
 *   β > 1: Stronger regularization, more disentanglement
 *   β < 1: Focus on reconstruction, less regularization
 *   β = 1: Standard VAE
 * ```
 *
 * BIO_MODULE: 0x1F04
 */

#ifndef NIMCP_VAE_LOSS_H
#define NIMCP_VAE_LOSS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default β value for standard VAE */
#define VAE_LOSS_DEFAULT_BETA           1.0f

/** Default free bits per dimension */
#define VAE_LOSS_DEFAULT_FREE_BITS      0.0f

/** Epsilon for numerical stability in log computations */
#define VAE_LOSS_LOG_EPSILON            1e-8f

/** Minimum variance for Gaussian NLL */
#define VAE_LOSS_MIN_VAR                1e-6f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Reconstruction loss types
 */
typedef enum vae_recon_loss_type {
    VAE_RECON_MSE = 0,           /**< Mean Squared Error (continuous) */
    VAE_RECON_BCE,               /**< Binary Cross Entropy (binary) */
    VAE_RECON_GAUSSIAN_NLL,      /**< Gaussian Negative Log Likelihood */
    VAE_RECON_LAPLACE_NLL,       /**< Laplace Negative Log Likelihood (robust) */
    VAE_RECON_HUBER,             /**< Huber loss (robust to outliers) */
    VAE_RECON_TYPE_COUNT
} vae_recon_loss_type_t;

/**
 * @brief KL loss reduction modes
 */
typedef enum vae_kl_reduction {
    VAE_KL_SUM = 0,              /**< Sum over dimensions */
    VAE_KL_MEAN,                 /**< Mean over dimensions */
    VAE_KL_NONE                  /**< No reduction (per-dimension) */
} vae_kl_reduction_t;

/**
 * @brief Loss aggregation modes
 */
typedef enum vae_loss_aggregation {
    VAE_LOSS_AGG_SUM = 0,        /**< Sum over batch */
    VAE_LOSS_AGG_MEAN            /**< Mean over batch */
} vae_loss_aggregation_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Loss computation configuration
 */
typedef struct vae_loss_config {
    /* Reconstruction loss */
    vae_recon_loss_type_t recon_type;    /**< Reconstruction loss type */
    float recon_weight;                   /**< Reconstruction loss weight */

    /* KL divergence */
    float beta;                           /**< β-VAE weight for KL */
    float free_bits;                      /**< Free bits per dimension */
    vae_kl_reduction_t kl_reduction;      /**< KL reduction mode */

    /* Aggregation */
    vae_loss_aggregation_t aggregation;   /**< Batch aggregation mode */

    /* Huber loss specific */
    float huber_delta;                    /**< Huber delta parameter */

    /* Precision weighting (FEP) */
    bool use_precision_weighting;         /**< Enable precision weighting */
    float min_precision;                  /**< Minimum precision value */
    float max_precision;                  /**< Maximum precision value */

    /* Annealing */
    bool use_kl_annealing;               /**< Enable KL annealing */
    float kl_anneal_start;               /**< KL weight at start */
    float kl_anneal_end;                 /**< KL weight at end */
    uint32_t kl_anneal_steps;            /**< Annealing steps */

    /* Cyclical annealing */
    bool use_cyclical_annealing;         /**< Enable cyclical annealing */
    uint32_t cycle_steps;                /**< Steps per cycle */
    uint32_t num_cycles;                 /**< Number of cycles */
} vae_loss_config_t;

/**
 * @brief Detailed loss breakdown
 */
typedef struct vae_loss_breakdown {
    float total_loss;                    /**< Total combined loss */
    float elbo;                          /**< Evidence lower bound */
    float free_energy;                   /**< Variational free energy (-ELBO) */

    /* Component losses */
    float recon_loss;                    /**< Reconstruction loss */
    float kl_loss;                       /**< KL divergence (after β weighting) */
    float kl_raw;                        /**< Raw KL divergence (before β) */

    /* Per-dimension KL (optional) */
    float* kl_per_dim;                   /**< KL per latent dimension */
    uint32_t latent_dim;                 /**< Latent dimension count */

    /* Batch statistics */
    uint32_t batch_size;                 /**< Batch size */
    float recon_per_sample;              /**< Average recon loss per sample */
    float kl_per_sample;                 /**< Average KL per sample */

    /* Active unit tracking */
    uint32_t active_units;               /**< Number of active KL units */
    float active_ratio;                  /**< Ratio of active units */

    /* Annealing state */
    float current_beta;                  /**< Current effective β */
    uint32_t current_step;               /**< Current training step */
} vae_loss_breakdown_t;

/**
 * @brief Loss computation context
 */
typedef struct vae_loss_ctx {
    vae_loss_config_t config;            /**< Configuration */

    /* Training state */
    uint32_t step;                       /**< Current step */
    float current_kl_weight;             /**< Current KL weight (annealing) */

    /* Running statistics */
    float ema_recon;                     /**< EMA of recon loss */
    float ema_kl;                        /**< EMA of KL loss */
    float ema_alpha;                     /**< EMA smoothing factor */

    /* Health monitoring */
    uint64_t loss_computations;          /**< Total computations */
    uint32_t nan_count;                  /**< NaN loss count */
    uint32_t inf_count;                  /**< Inf loss count */

    /* Thread safety */
    nimcp_mutex_t* mutex;

    bool is_initialized;
} vae_loss_ctx_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default loss configuration
 *
 * @return Default configuration
 */
vae_loss_config_t vae_loss_default_config(void);

/**
 * @brief Create loss computation context
 *
 * @param config Loss configuration
 * @return Context or NULL on error
 */
vae_loss_ctx_t* vae_loss_ctx_create(const vae_loss_config_t* config);

/**
 * @brief Destroy loss context
 *
 * @param ctx Context to destroy (NULL safe)
 */
void vae_loss_ctx_destroy(vae_loss_ctx_t* ctx);

/**
 * @brief Reset loss context state
 *
 * @param ctx Context to reset
 * @return 0 on success, -1 on error
 */
int vae_loss_ctx_reset(vae_loss_ctx_t* ctx);

/* ============================================================================
 * Core Loss Computation API
 * ============================================================================ */

/**
 * @brief Compute complete VAE loss
 *
 * Computes ELBO = -recon_loss - β * KL
 *
 * @param ctx Loss context
 * @param x Original input [batch_size, input_dim]
 * @param x_recon Reconstruction [batch_size, input_dim]
 * @param mu Latent mean [batch_size, latent_dim]
 * @param log_var Latent log variance [batch_size, latent_dim]
 * @param breakdown Output detailed breakdown (optional, can be NULL)
 * @return Total loss value, or NAN on error
 */
float vae_loss_compute(vae_loss_ctx_t* ctx,
                       const nimcp_tensor_t* x,
                       const nimcp_tensor_t* x_recon,
                       const nimcp_tensor_t* mu,
                       const nimcp_tensor_t* log_var,
                       vae_loss_breakdown_t* breakdown);

/**
 * @brief Compute loss without context (stateless)
 *
 * @param config Loss configuration
 * @param x Original input
 * @param x_recon Reconstruction
 * @param mu Latent mean
 * @param log_var Latent log variance
 * @return Total loss value
 */
float vae_loss_compute_stateless(const vae_loss_config_t* config,
                                 const nimcp_tensor_t* x,
                                 const nimcp_tensor_t* x_recon,
                                 const nimcp_tensor_t* mu,
                                 const nimcp_tensor_t* log_var);

/* ============================================================================
 * Reconstruction Loss API
 * ============================================================================ */

/**
 * @brief Compute MSE reconstruction loss
 *
 * MSE = (1/N) * sum((x - x_recon)^2)
 *
 * @param x Original [batch_size, dim]
 * @param x_recon Reconstruction [batch_size, dim]
 * @param reduction Reduction mode (sum/mean over batch)
 * @return Loss value
 */
float vae_loss_mse(const nimcp_tensor_t* x,
                   const nimcp_tensor_t* x_recon,
                   vae_loss_aggregation_t reduction);

/**
 * @brief Compute Binary Cross Entropy loss
 *
 * BCE = -sum(x * log(x_recon) + (1-x) * log(1-x_recon))
 *
 * @param x Original (values in [0,1])
 * @param x_recon Reconstruction (values in [0,1])
 * @param reduction Reduction mode
 * @return Loss value
 */
float vae_loss_bce(const nimcp_tensor_t* x,
                   const nimcp_tensor_t* x_recon,
                   vae_loss_aggregation_t reduction);

/**
 * @brief Compute Gaussian Negative Log Likelihood
 *
 * NLL = 0.5 * (log(var) + (x - mu)^2 / var)
 *
 * @param x Original
 * @param recon_mu Reconstruction mean
 * @param recon_log_var Reconstruction log variance
 * @param reduction Reduction mode
 * @return Loss value
 */
float vae_loss_gaussian_nll(const nimcp_tensor_t* x,
                            const nimcp_tensor_t* recon_mu,
                            const nimcp_tensor_t* recon_log_var,
                            vae_loss_aggregation_t reduction);

/**
 * @brief Compute Laplace Negative Log Likelihood
 *
 * More robust to outliers than Gaussian.
 * NLL = log(2b) + |x - mu| / b
 *
 * @param x Original
 * @param recon_mu Reconstruction mean
 * @param scale Laplace scale parameter
 * @param reduction Reduction mode
 * @return Loss value
 */
float vae_loss_laplace_nll(const nimcp_tensor_t* x,
                           const nimcp_tensor_t* recon_mu,
                           float scale,
                           vae_loss_aggregation_t reduction);

/**
 * @brief Compute Huber loss
 *
 * Quadratic for small errors, linear for large errors.
 *
 * @param x Original
 * @param x_recon Reconstruction
 * @param delta Huber delta
 * @param reduction Reduction mode
 * @return Loss value
 */
float vae_loss_huber(const nimcp_tensor_t* x,
                     const nimcp_tensor_t* x_recon,
                     float delta,
                     vae_loss_aggregation_t reduction);

/* ============================================================================
 * KL Divergence API
 * ============================================================================ */

/**
 * @brief Compute KL divergence from standard normal
 *
 * KL[N(mu, sigma^2) || N(0, I)] = -0.5 * sum(1 + log_var - mu^2 - exp(log_var))
 *
 * @param mu Mean [batch_size, latent_dim]
 * @param log_var Log variance [batch_size, latent_dim]
 * @param reduction KL reduction mode
 * @param batch_reduction Batch aggregation mode
 * @return KL divergence value
 */
float vae_loss_kl_standard_normal(const nimcp_tensor_t* mu,
                                  const nimcp_tensor_t* log_var,
                                  vae_kl_reduction_t reduction,
                                  vae_loss_aggregation_t batch_reduction);

/**
 * @brief Compute KL with free bits
 *
 * Ensures minimum KL per dimension: KL_i = max(free_bits, KL_i)
 *
 * @param mu Mean
 * @param log_var Log variance
 * @param free_bits Minimum KL per dimension
 * @param kl_per_dim Output per-dimension KL (optional)
 * @param batch_reduction Batch aggregation mode
 * @return Total KL with free bits
 */
float vae_loss_kl_with_free_bits(const nimcp_tensor_t* mu,
                                 const nimcp_tensor_t* log_var,
                                 float free_bits,
                                 float* kl_per_dim,
                                 vae_loss_aggregation_t batch_reduction);

/**
 * @brief Compute per-dimension KL divergence
 *
 * @param mu Mean
 * @param log_var Log variance
 * @param kl_per_dim Output array [latent_dim]
 * @return 0 on success, -1 on error
 */
int vae_loss_kl_per_dimension(const nimcp_tensor_t* mu,
                              const nimcp_tensor_t* log_var,
                              float* kl_per_dim);

/* ============================================================================
 * FEP Integration API
 * ============================================================================ */

/**
 * @brief Compute variational free energy
 *
 * F = E_q[-log p(x|z)] + KL[q(z|x) || p(z)]
 *   = Prediction Error + Complexity Cost
 *
 * @param ctx Loss context
 * @param x Original input
 * @param x_recon Reconstruction
 * @param mu Latent mean
 * @param log_var Latent log variance
 * @param prediction_error Output prediction error component
 * @param complexity_cost Output complexity cost component
 * @return Variational free energy
 */
float vae_loss_free_energy(vae_loss_ctx_t* ctx,
                           const nimcp_tensor_t* x,
                           const nimcp_tensor_t* x_recon,
                           const nimcp_tensor_t* mu,
                           const nimcp_tensor_t* log_var,
                           float* prediction_error,
                           float* complexity_cost);

/**
 * @brief Compute precision-weighted reconstruction loss
 *
 * Weighted loss = precision * (x - x_recon)^2
 *
 * @param x Original
 * @param x_recon Reconstruction
 * @param precision Precision tensor [batch_size, dim] or scalar
 * @param reduction Reduction mode
 * @return Precision-weighted loss
 */
float vae_loss_precision_weighted(const nimcp_tensor_t* x,
                                  const nimcp_tensor_t* x_recon,
                                  const nimcp_tensor_t* precision,
                                  vae_loss_aggregation_t reduction);

/**
 * @brief Get free energy components for FEP module
 *
 * Provides breakdown suitable for FEP integration.
 *
 * @param breakdown Loss breakdown
 * @param fep_free_energy Output FEP-compatible free energy
 * @param fep_accuracy Output accuracy term (negative recon loss)
 * @param fep_complexity Output complexity term (KL)
 */
void vae_loss_to_fep(const vae_loss_breakdown_t* breakdown,
                     float* fep_free_energy,
                     float* fep_accuracy,
                     float* fep_complexity);

/* ============================================================================
 * Annealing API
 * ============================================================================ */

/**
 * @brief Update annealing step
 *
 * @param ctx Loss context
 * @return Current KL weight after update
 */
float vae_loss_anneal_step(vae_loss_ctx_t* ctx);

/**
 * @brief Get current KL weight
 *
 * @param ctx Loss context
 * @return Current KL weight
 */
float vae_loss_get_kl_weight(const vae_loss_ctx_t* ctx);

/**
 * @brief Set current step for annealing
 *
 * @param ctx Loss context
 * @param step Step number
 * @return 0 on success, -1 on error
 */
int vae_loss_set_step(vae_loss_ctx_t* ctx, uint32_t step);

/**
 * @brief Compute linear annealing weight
 *
 * @param step Current step
 * @param start_value Starting value
 * @param end_value Ending value
 * @param total_steps Total annealing steps
 * @return Annealed weight
 */
float vae_loss_linear_anneal(uint32_t step,
                             float start_value,
                             float end_value,
                             uint32_t total_steps);

/**
 * @brief Compute cyclical annealing weight
 *
 * @param step Current step
 * @param cycle_steps Steps per cycle
 * @param num_cycles Total cycles
 * @param total_steps Total training steps
 * @return Cyclical weight
 */
float vae_loss_cyclical_anneal(uint32_t step,
                               uint32_t cycle_steps,
                               uint32_t num_cycles,
                               uint32_t total_steps);

/* ============================================================================
 * Gradient Computation API
 * ============================================================================ */

/**
 * @brief Compute reconstruction loss gradient
 *
 * @param x Original
 * @param x_recon Reconstruction
 * @param loss_type Loss type
 * @param d_recon Output gradient w.r.t. reconstruction
 * @return 0 on success, -1 on error
 */
int vae_loss_recon_gradient(const nimcp_tensor_t* x,
                            const nimcp_tensor_t* x_recon,
                            vae_recon_loss_type_t loss_type,
                            nimcp_tensor_t* d_recon);

/**
 * @brief Compute KL divergence gradients
 *
 * d_mu = mu
 * d_log_var = 0.5 * (exp(log_var) - 1)
 *
 * @param mu Mean
 * @param log_var Log variance
 * @param beta β weight
 * @param d_mu Output gradient w.r.t. mu
 * @param d_log_var Output gradient w.r.t. log_var
 * @return 0 on success, -1 on error
 */
int vae_loss_kl_gradient(const nimcp_tensor_t* mu,
                         const nimcp_tensor_t* log_var,
                         float beta,
                         nimcp_tensor_t* d_mu,
                         nimcp_tensor_t* d_log_var);

/* ============================================================================
 * Monitoring API
 * ============================================================================ */

/**
 * @brief Allocate loss breakdown structure
 *
 * @param latent_dim Latent dimension for per-dim KL
 * @return Allocated breakdown or NULL
 */
vae_loss_breakdown_t* vae_loss_breakdown_create(uint32_t latent_dim);

/**
 * @brief Free loss breakdown
 *
 * @param breakdown Breakdown to free (NULL safe)
 */
void vae_loss_breakdown_free(vae_loss_breakdown_t* breakdown);

/**
 * @brief Check for numerical issues in loss
 *
 * @param loss Loss value
 * @return true if loss is NaN or Inf
 */
bool vae_loss_is_invalid(float loss);

/**
 * @brief Get loss context statistics
 *
 * @param ctx Loss context
 * @param nan_count Output NaN count
 * @param inf_count Output Inf count
 * @param total_computations Output total computations
 */
void vae_loss_get_stats(const vae_loss_ctx_t* ctx,
                        uint32_t* nan_count,
                        uint32_t* inf_count,
                        uint64_t* total_computations);

/**
 * @brief Count active latent units from KL breakdown
 *
 * A unit is active if its KL > threshold.
 *
 * @param kl_per_dim Per-dimension KL array
 * @param latent_dim Number of dimensions
 * @param threshold Activity threshold
 * @return Number of active units
 */
uint32_t vae_loss_count_active_units(const float* kl_per_dim,
                                     uint32_t latent_dim,
                                     float threshold);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for loss context
 *
 * @param ctx Loss context
 * @param agent Health agent
 */
void vae_loss_set_health_agent(vae_loss_ctx_t* ctx,
                               nimcp_health_agent_t* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_LOSS_H */
