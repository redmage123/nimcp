/**
 * @file nimcp_vae_latent.h
 * @brief VAE Latent Space Operations
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Operations on the VAE latent space including sampling,
 *       reparameterization, and distribution utilities.
 *
 * WHY:  The latent space is where VAE stores compressed representations.
 *       Proper handling of sampling and distribution parameters is
 *       critical for both training and generation.
 *
 * HOW:  - Reparameterization trick: z = mu + sigma * epsilon
 *       - Variance bounds checking to prevent collapse/explosion
 *       - KL divergence computation
 *       - Precision (inverse variance) for FEP integration
 *
 * REPARAMETERIZATION TRICK:
 * ```
 *   Instead of sampling:  z ~ N(mu, sigma^2)
 *   We compute:           z = mu + sigma * epsilon
 *   Where:                epsilon ~ N(0, 1)
 *
 *   This allows gradients to flow through the sampling operation.
 * ```
 *
 * BIO_MODULE: 0x1F03
 */

#ifndef NIMCP_VAE_LATENT_H
#define NIMCP_VAE_LATENT_H

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

/** Log variance clamp bounds to prevent numerical issues */
#define VAE_LATENT_MIN_LOG_VAR          -10.0f
#define VAE_LATENT_MAX_LOG_VAR          10.0f

/** Variance threshold for detecting posterior collapse */
#define VAE_LATENT_COLLAPSE_THRESHOLD   0.01f

/** Variance threshold for detecting explosion */
#define VAE_LATENT_EXPLODE_THRESHOLD    50.0f

/* ============================================================================
 * Sampling API
 * ============================================================================ */

/**
 * @brief Sample from latent distribution using reparameterization trick
 *
 * Computes z = mu + exp(0.5 * log_var) * epsilon
 * where epsilon ~ N(0, I)
 *
 * @param mu Mean tensor [batch_size, latent_dim]
 * @param log_var Log variance tensor [batch_size, latent_dim]
 * @param z Output samples [batch_size, latent_dim]
 * @return 0 on success, -1 on error
 */
int vae_latent_sample(const nimcp_tensor_t* mu,
                      const nimcp_tensor_t* log_var,
                      nimcp_tensor_t* z);

/**
 * @brief Sample from standard normal prior
 *
 * Generates samples from N(0, I)
 *
 * @param num_samples Number of samples to generate
 * @param latent_dim Dimension of each sample
 * @param samples Output tensor [num_samples, latent_dim]
 * @return 0 on success, -1 on error
 */
int vae_latent_sample_prior(uint32_t num_samples,
                            uint32_t latent_dim,
                            nimcp_tensor_t* samples);

/**
 * @brief Sample with provided random noise
 *
 * Useful for reproducibility when epsilon is provided externally.
 *
 * @param mu Mean tensor
 * @param log_var Log variance tensor
 * @param epsilon Pre-generated noise [batch_size, latent_dim]
 * @param z Output samples
 * @return 0 on success, -1 on error
 */
int vae_latent_sample_with_noise(const nimcp_tensor_t* mu,
                                 const nimcp_tensor_t* log_var,
                                 const nimcp_tensor_t* epsilon,
                                 nimcp_tensor_t* z);

/* ============================================================================
 * Distribution Operations
 * ============================================================================ */

/**
 * @brief Compute KL divergence from standard normal
 *
 * KL[N(mu, sigma^2) || N(0, I)] = -0.5 * sum(1 + log_var - mu^2 - exp(log_var))
 *
 * @param mu Mean tensor [batch_size, latent_dim]
 * @param log_var Log variance tensor [batch_size, latent_dim]
 * @param kl_per_dim Output KL per dimension [latent_dim] (optional, can be NULL)
 * @return Total KL divergence (summed over dimensions, averaged over batch)
 */
float vae_latent_kl_divergence(const nimcp_tensor_t* mu,
                               const nimcp_tensor_t* log_var,
                               float* kl_per_dim);

/**
 * @brief Compute KL divergence with free bits
 *
 * Free bits ensures minimum KL per dimension to prevent collapse:
 * KL_i = max(free_bits, KL_i)
 *
 * @param mu Mean tensor
 * @param log_var Log variance tensor
 * @param free_bits Minimum KL per dimension
 * @param kl_per_dim Output KL per dimension (optional)
 * @return Total KL divergence with free bits applied
 */
float vae_latent_kl_with_free_bits(const nimcp_tensor_t* mu,
                                   const nimcp_tensor_t* log_var,
                                   float free_bits,
                                   float* kl_per_dim);

/**
 * @brief Compute precision from log variance
 *
 * Precision = 1 / variance = exp(-log_var)
 *
 * @param log_var Log variance tensor
 * @param precision Output precision tensor (same shape as log_var)
 * @return 0 on success, -1 on error
 */
int vae_latent_compute_precision(const nimcp_tensor_t* log_var,
                                 nimcp_tensor_t* precision);

/**
 * @brief Get average precision across dimensions
 *
 * @param log_var Log variance tensor
 * @return Average precision, or NAN on error
 */
float vae_latent_avg_precision(const nimcp_tensor_t* log_var);

/* ============================================================================
 * Variance Monitoring
 * ============================================================================ */

/**
 * @brief Check for posterior collapse
 *
 * Posterior collapse occurs when latent dimensions become inactive
 * (variance approaches 1, KL approaches 0).
 *
 * @param mu Mean tensor
 * @param log_var Log variance tensor
 * @param threshold Collapse threshold (default: VAE_LATENT_COLLAPSE_THRESHOLD)
 * @param num_collapsed Output number of collapsed dimensions
 * @return true if any dimension is collapsed
 */
bool vae_latent_check_collapse(const nimcp_tensor_t* mu,
                               const nimcp_tensor_t* log_var,
                               float threshold,
                               uint32_t* num_collapsed);

/**
 * @brief Check for variance explosion
 *
 * @param log_var Log variance tensor
 * @param threshold Explosion threshold
 * @param num_exploded Output number of exploded dimensions
 * @return true if any dimension has exploded variance
 */
bool vae_latent_check_explosion(const nimcp_tensor_t* log_var,
                                float threshold,
                                uint32_t* num_exploded);

/**
 * @brief Get variance statistics
 *
 * @param log_var Log variance tensor
 * @param min_var Output minimum variance
 * @param max_var Output maximum variance
 * @param avg_var Output average variance
 * @return 0 on success, -1 on error
 */
int vae_latent_variance_stats(const nimcp_tensor_t* log_var,
                              float* min_var,
                              float* max_var,
                              float* avg_var);

/**
 * @brief Count active latent dimensions
 *
 * A dimension is considered active if KL > threshold.
 *
 * @param mu Mean tensor
 * @param log_var Log variance tensor
 * @param threshold Activity threshold
 * @return Number of active dimensions
 */
uint32_t vae_latent_count_active(const nimcp_tensor_t* mu,
                                 const nimcp_tensor_t* log_var,
                                 float threshold);

/* ============================================================================
 * Interpolation Operations
 * ============================================================================ */

/**
 * @brief Linear interpolation between two latent points
 *
 * z_interp = (1 - alpha) * z1 + alpha * z2
 *
 * @param z1 First latent point [latent_dim]
 * @param z2 Second latent point [latent_dim]
 * @param alpha Interpolation factor [0, 1]
 * @param z_interp Output interpolated point [latent_dim]
 * @return 0 on success, -1 on error
 */
int vae_latent_lerp(const nimcp_tensor_t* z1,
                    const nimcp_tensor_t* z2,
                    float alpha,
                    nimcp_tensor_t* z_interp);

/**
 * @brief Spherical linear interpolation (slerp)
 *
 * Interpolates along the great circle on the hypersphere.
 *
 * @param z1 First latent point
 * @param z2 Second latent point
 * @param alpha Interpolation factor [0, 1]
 * @param z_interp Output interpolated point
 * @return 0 on success, -1 on error
 */
int vae_latent_slerp(const nimcp_tensor_t* z1,
                     const nimcp_tensor_t* z2,
                     float alpha,
                     nimcp_tensor_t* z_interp);

/**
 * @brief Generate interpolation path
 *
 * @param z1 First latent point
 * @param z2 Second latent point
 * @param num_steps Number of interpolation steps
 * @param use_slerp Use spherical interpolation
 * @param path Output path [num_steps, latent_dim]
 * @return 0 on success, -1 on error
 */
int vae_latent_interpolate_path(const nimcp_tensor_t* z1,
                                const nimcp_tensor_t* z2,
                                uint32_t num_steps,
                                bool use_slerp,
                                nimcp_tensor_t* path);

/* ============================================================================
 * Latent State Management
 * ============================================================================ */

/**
 * @brief Initialize latent state structure
 *
 * @param state State to initialize
 * @param latent_dim Latent dimension
 * @return 0 on success, -1 on error
 */
int vae_latent_state_init(vae_latent_state_t* state, uint32_t latent_dim);

/**
 * @brief Free latent state internal buffers
 *
 * @param state State to free
 */
void vae_latent_state_free(vae_latent_state_t* state);

/**
 * @brief Update latent state from tensors
 *
 * @param state State to update
 * @param mu Mean tensor
 * @param log_var Log variance tensor
 * @param z Sample tensor
 * @return 0 on success, -1 on error
 */
int vae_latent_state_update(vae_latent_state_t* state,
                            const nimcp_tensor_t* mu,
                            const nimcp_tensor_t* log_var,
                            const nimcp_tensor_t* z);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_LATENT_H */
