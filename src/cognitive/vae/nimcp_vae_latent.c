/**
 * @file nimcp_vae_latent.c
 * @brief VAE Latent Space Operations Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Implementation of latent space operations for VAE.
 *
 * WHY:  Core operations for sampling, distribution computations,
 *       and latent space manipulation.
 *
 * HOW:  Reparameterization trick for differentiable sampling,
 *       closed-form KL divergence, precision computation.
 */

#include "cognitive/vae/nimcp_vae_latent.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "utils/math/nimcp_math_helpers.h"
#include "utils/geometry/nimcp_differential_geometry.h"

BRIDGE_BOILERPLATE_MESH_ONLY(vae_latent, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "VAE_LATENT"

static inline bool is_nan_f(float value) {
    return value != value;
}

/**
 * @brief Generate random number from standard normal (Box-Muller)
 */
static float randn(void) {
    static bool has_spare = false;
    static float spare;

    if (has_spare) {
        has_spare = false;
        return spare;
    }

    float u, v, s;
    do {
        u = ((float)nimcp_tl_rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        v = ((float)nimcp_tl_rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        s = u * u + v * v;
    } while (s >= 1.0f || s == 0.0f);

    s = sqrtf(-2.0f * logf(s) / s);
    spare = v * s;
    has_spare = true;
    return u * s;
}

/**
 * @brief Compute dot product of two vectors
 */
static float dot_product(const float* a, const float* b, uint32_t dim) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

/**
 * @brief Compute L2 norm of vector
 */
static float vector_norm(const float* v, uint32_t dim) {
    return sqrtf(dot_product(v, v, dim));
}

/* ============================================================================
 * Sampling Implementation
 * ============================================================================ */

int vae_latent_sample(const nimcp_tensor_t* mu,
                      const nimcp_tensor_t* log_var,
                      nimcp_tensor_t* z) {
    if (!mu || !log_var || !z) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_latent_sample");
        return -1;
    }

    vae_latent_heartbeat("vae_latent_sample", 0.0f);

    /* Get dimensions */
    size_t total_size = nimcp_tensor_size(mu);

    float* mu_data = (float*)mu->data;
    float* logvar_data = (float*)log_var->data;
    float* z_data = (float*)z->data;

    /* Reparameterization trick: z = mu + exp(0.5 * log_var) * epsilon */
    for (size_t i = 0; i < total_size; i++) {
        float log_var_clamped = nimcp_clampf(logvar_data[i],
                                        VAE_LATENT_MIN_LOG_VAR,
                                        VAE_LATENT_MAX_LOG_VAR);
        float std = expf(0.5f * log_var_clamped);
        float epsilon = randn();
        z_data[i] = mu_data[i] + std * epsilon;

        /* Check for NaN */
        if (is_nan_f(z_data[i])) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_SAMPLE_FAILED,
                                  "NaN in latent sample at index %zu", i);
            return -1;
        }

        if ((i & 0xFF) == 0 && total_size > 256) {
            vae_latent_heartbeat("vae_latent_sample", (float)i / (float)total_size);
        }
    }

    vae_latent_heartbeat("vae_latent_sample", 1.0f);

    return 0;
}

int vae_latent_sample_prior(uint32_t num_samples,
                            uint32_t latent_dim,
                            nimcp_tensor_t* samples) {
    if (!samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_latent_sample_prior");
        return -1;
    }

    if (num_samples == 0 || latent_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_DIM,
                              "Invalid dimensions in vae_latent_sample_prior");
        return -1;
    }

    vae_latent_heartbeat("vae_latent_sample_prior", 0.0f);

    float* data = (float*)samples->data;
    size_t total_size = (size_t)num_samples * latent_dim;

    for (size_t i = 0; i < total_size; i++) {
        data[i] = randn();

        if ((i & 0x3FF) == 0 && total_size > 1024) {
            vae_latent_heartbeat("vae_latent_sample_prior", (float)i / (float)total_size);
        }
    }

    vae_latent_heartbeat("vae_latent_sample_prior", 1.0f);

    return 0;
}

int vae_latent_sample_with_noise(const nimcp_tensor_t* mu,
                                 const nimcp_tensor_t* log_var,
                                 const nimcp_tensor_t* epsilon,
                                 nimcp_tensor_t* z) {
    if (!mu || !log_var || !epsilon || !z) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_latent_sample_with_noise");
        return -1;
    }

    size_t total_size = nimcp_tensor_size(mu);

    float* mu_data = (float*)mu->data;
    float* logvar_data = (float*)log_var->data;
    float* eps_data = (float*)epsilon->data;
    float* z_data = (float*)z->data;

    for (size_t i = 0; i < total_size; i++) {
        float log_var_clamped = nimcp_clampf(logvar_data[i],
                                        VAE_LATENT_MIN_LOG_VAR,
                                        VAE_LATENT_MAX_LOG_VAR);
        float std = expf(0.5f * log_var_clamped);
        z_data[i] = mu_data[i] + std * eps_data[i];
    }

    return 0;
}

/* ============================================================================
 * Distribution Operations Implementation
 * ============================================================================ */

/**
 * @brief VAE-specific KL divergence: KL[N(μ, σ²) || N(0, 1)]
 *
 * NOTE: This is a closed-form analytical KL divergence specific to VAEs.
 * It computes: -0.5 * sum(1 + log_var - mu^2 - exp(log_var))
 *
 * This is DIFFERENT from nimcp_stats_kl_divergence() in the central statistics
 * module, which computes general discrete KL divergence D_KL(P||Q) between
 * two probability distributions. The VAE formula is the analytical solution
 * for Gaussian-to-Gaussian KL when the target is the standard normal N(0,1).
 *
 * DO NOT replace with nimcp_stats_kl_divergence() - they serve different purposes.
 */
float vae_latent_kl_divergence(const nimcp_tensor_t* mu,
                               const nimcp_tensor_t* log_var,
                               float* kl_per_dim) {
    if (!mu || !log_var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_latent_kl_divergence");
        return NAN;
    }

    vae_latent_heartbeat("vae_latent_kl_divergence", 0.0f);

    /* Get dimensions */
    uint32_t batch_size = mu->shape.dims[0];
    uint32_t latent_dim = (mu->shape.rank > 1) ? mu->shape.dims[1] : mu->shape.dims[0];

    if (mu->shape.rank == 1) {
        batch_size = 1;
    }

    float* mu_data = (float*)mu->data;
    float* logvar_data = (float*)log_var->data;

    /* KL[N(mu, sigma^2) || N(0, 1)] = -0.5 * sum(1 + log_var - mu^2 - exp(log_var)) */
    float total_kl = 0.0f;

    /* Initialize per-dim KL if requested */
    if (kl_per_dim) {
        memset(kl_per_dim, 0, latent_dim * sizeof(float));
    }

    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t d = 0; d < latent_dim; d++) {
            size_t idx = b * latent_dim + d;
            float m = mu_data[idx];
            float lv = nimcp_clampf(logvar_data[idx], VAE_LATENT_MIN_LOG_VAR, VAE_LATENT_MAX_LOG_VAR);

            /* KL for single dimension: -0.5 * (1 + log_var - mu^2 - exp(log_var)) */
            float kl_d = -0.5f * (1.0f + lv - m * m - expf(lv));

            total_kl += kl_d;

            if (kl_per_dim) {
                kl_per_dim[d] += kl_d / (float)batch_size;
            }
        }

        if ((b & 0x3F) == 0 && batch_size > 64) {
            vae_latent_heartbeat("vae_latent_kl_divergence", (float)b / (float)batch_size);
        }
    }

    /* Average over batch */
    total_kl /= (float)batch_size;

    vae_latent_heartbeat("vae_latent_kl_divergence", 1.0f);

    return total_kl;
}

float vae_latent_kl_with_free_bits(const nimcp_tensor_t* mu,
                                   const nimcp_tensor_t* log_var,
                                   float free_bits,
                                   float* kl_per_dim) {
    if (!mu || !log_var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_latent_kl_with_free_bits");
        return NAN;
    }

    uint32_t batch_size = mu->shape.dims[0];
    uint32_t latent_dim = (mu->shape.rank > 1) ? mu->shape.dims[1] : mu->shape.dims[0];

    if (mu->shape.rank == 1) {
        batch_size = 1;
    }

    /* First compute per-dimension KL */
    float* kl_dims = (float*)nimcp_calloc(latent_dim, sizeof(float));
    if (!kl_dims) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate KL buffer");
        return NAN;
    }

    /* Compute KL per dimension (averaged over batch) */
    vae_latent_kl_divergence(mu, log_var, kl_dims);

    /* Apply free bits */
    float total_kl = 0.0f;
    for (uint32_t d = 0; d < latent_dim; d++) {
        float kl_d = kl_dims[d];

        /* Free bits: max(free_bits, KL_d) */
        if (kl_d < free_bits) {
            kl_d = free_bits;
        }

        total_kl += kl_d;

        if (kl_per_dim) {
            kl_per_dim[d] = kl_d;
        }
    }

    nimcp_free(kl_dims);
    kl_dims = NULL;

    return total_kl;
}

int vae_latent_compute_precision(const nimcp_tensor_t* log_var,
                                 nimcp_tensor_t* precision) {
    if (!log_var || !precision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_latent_compute_precision");
        return -1;
    }

    size_t total_size = nimcp_tensor_size(log_var);
    float* logvar_data = (float*)log_var->data;
    float* prec_data = (float*)precision->data;

    /* Precision = 1 / variance = exp(-log_var) */
    for (size_t i = 0; i < total_size; i++) {
        float lv = nimcp_clampf(logvar_data[i], VAE_LATENT_MIN_LOG_VAR, VAE_LATENT_MAX_LOG_VAR);
        prec_data[i] = expf(-lv);
    }

    return 0;
}

float vae_latent_avg_precision(const nimcp_tensor_t* log_var) {
    if (!log_var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_latent_avg_precision");
        return NAN;
    }

    size_t total_size = nimcp_tensor_size(log_var);
    float* logvar_data = (float*)log_var->data;

    float sum_precision = 0.0f;
    for (size_t i = 0; i < total_size; i++) {
        float lv = nimcp_clampf(logvar_data[i], VAE_LATENT_MIN_LOG_VAR, VAE_LATENT_MAX_LOG_VAR);
        sum_precision += expf(-lv);
    }

    return sum_precision / (float)total_size;
}

/* ============================================================================
 * Variance Monitoring Implementation
 * ============================================================================ */

bool vae_latent_check_collapse(const nimcp_tensor_t* mu,
                               const nimcp_tensor_t* log_var,
                               float threshold,
                               uint32_t* num_collapsed) {
    if (!mu || !log_var) {
        if (num_collapsed) *num_collapsed = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_latent_avg_precision: validation failed");
        return false;
    }

    if (threshold <= 0.0f) {
        threshold = VAE_LATENT_COLLAPSE_THRESHOLD;
    }

    uint32_t latent_dim = (mu->shape.rank > 1) ? mu->shape.dims[1] : mu->shape.dims[0];
    uint32_t batch_size = (mu->shape.rank > 1) ? mu->shape.dims[0] : 1;

    float* mu_data = (float*)mu->data;
    float* logvar_data = (float*)log_var->data;

    uint32_t collapsed = 0;

    /* Check each dimension for collapse */
    for (uint32_t d = 0; d < latent_dim; d++) {
        float sum_kl = 0.0f;

        for (uint32_t b = 0; b < batch_size; b++) {
            size_t idx = b * latent_dim + d;
            float m = mu_data[idx];
            float lv = logvar_data[idx];

            /* KL for this dimension */
            float kl = -0.5f * (1.0f + lv - m * m - expf(lv));
            sum_kl += kl;
        }

        float avg_kl = sum_kl / (float)batch_size;

        /* Dimension is collapsed if KL is below threshold */
        if (avg_kl < threshold) {
            collapsed++;
        }
    }

    if (num_collapsed) {
        *num_collapsed = collapsed;
    }

    return collapsed > 0;
}

bool vae_latent_check_explosion(const nimcp_tensor_t* log_var,
                                float threshold,
                                uint32_t* num_exploded) {
    if (!log_var) {
        if (num_exploded) *num_exploded = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_latent_avg_precision: validation failed");
        return false;
    }

    if (threshold <= 0.0f) {
        threshold = VAE_LATENT_EXPLODE_THRESHOLD;
    }

    size_t total_size = nimcp_tensor_size(log_var);
    float* logvar_data = (float*)log_var->data;

    float log_threshold = logf(threshold);
    uint32_t exploded = 0;

    for (size_t i = 0; i < total_size; i++) {
        if (logvar_data[i] > log_threshold) {
            exploded++;
        }
    }

    if (num_exploded) {
        *num_exploded = exploded;
    }

    return exploded > 0;
}

int vae_latent_variance_stats(const nimcp_tensor_t* log_var,
                              float* min_var,
                              float* max_var,
                              float* avg_var) {
    if (!log_var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_latent_variance_stats");
        return -1;
    }

    size_t total_size = nimcp_tensor_size(log_var);
    float* logvar_data = (float*)log_var->data;

    float min_v = FLT_MAX;
    float max_v = -FLT_MAX;
    float sum_v = 0.0f;

    for (size_t i = 0; i < total_size; i++) {
        float var = expf(logvar_data[i]);

        if (var < min_v) min_v = var;
        if (var > max_v) max_v = var;
        sum_v += var;
    }

    if (min_var) *min_var = min_v;
    if (max_var) *max_var = max_v;
    if (avg_var) *avg_var = sum_v / (float)total_size;

    return 0;
}

uint32_t vae_latent_count_active(const nimcp_tensor_t* mu,
                                 const nimcp_tensor_t* log_var,
                                 float threshold) {
    if (!mu || !log_var) return 0;

    if (threshold <= 0.0f) {
        threshold = 0.1f;  /* Default activity threshold */
    }

    uint32_t latent_dim = (mu->shape.rank > 1) ? mu->shape.dims[1] : mu->shape.dims[0];
    uint32_t batch_size = (mu->shape.rank > 1) ? mu->shape.dims[0] : 1;

    float* mu_data = (float*)mu->data;
    float* logvar_data = (float*)log_var->data;

    uint32_t active = 0;

    for (uint32_t d = 0; d < latent_dim; d++) {
        float sum_kl = 0.0f;

        for (uint32_t b = 0; b < batch_size; b++) {
            size_t idx = b * latent_dim + d;
            float m = mu_data[idx];
            float lv = logvar_data[idx];

            float kl = -0.5f * (1.0f + lv - m * m - expf(lv));
            sum_kl += kl;
        }

        float avg_kl = sum_kl / (float)batch_size;

        if (avg_kl > threshold) {
            active++;
        }
    }

    return active;
}

/* ============================================================================
 * Interpolation Implementation
 * ============================================================================ */

int vae_latent_lerp(const nimcp_tensor_t* z1,
                    const nimcp_tensor_t* z2,
                    float alpha,
                    nimcp_tensor_t* z_interp) {
    if (!z1 || !z2 || !z_interp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_latent_lerp");
        return -1;
    }

    size_t size = nimcp_tensor_size(z1);
    float* z1_data = (float*)z1->data;
    float* z2_data = (float*)z2->data;
    float* interp_data = (float*)z_interp->data;

    float one_minus_alpha = 1.0f - alpha;

    for (size_t i = 0; i < size; i++) {
        interp_data[i] = one_minus_alpha * z1_data[i] + alpha * z2_data[i];
    }

    return 0;
}

int vae_latent_slerp(const nimcp_tensor_t* z1,
                     const nimcp_tensor_t* z2,
                     float alpha,
                     nimcp_tensor_t* z_interp) {
    if (!z1 || !z2 || !z_interp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_latent_slerp");
        return -1;
    }

    uint32_t dim = (z1->shape.rank > 1) ? z1->shape.dims[1] : z1->shape.dims[0];

    float* z1_data = (float*)z1->data;
    float* z2_data = (float*)z2->data;
    float* interp_data = (float*)z_interp->data;

    /* For small latent spaces (dim ≤ 64), use Riemannian geodesic interpolation
     * with the Fisher-Rao metric instead of flat SLERP. The Fisher metric for
     * Gaussian latent codes makes the interpolation aware of the information
     * geometry of the distribution, producing smoother latent traversals that
     * better preserve semantic meaning.
     *
     * Fisher metric for isotropic Gaussian at z: G_ii = 1 + z_i^2
     * (contribution from both mean and variance parameterization) */
    if (dim <= DIFFGEO_MAX_DIM) {
        riemannian_metric_t* fisher = riemannian_metric_create(dim);
        if (fisher) {
            /* Build Fisher metric at midpoint (average of z1 and z2) */
            for (uint32_t i = 0; i < dim; i++) {
                float z_mid = 0.5f * (z1_data[i] + z2_data[i]);
                fisher->g[i * dim + i] = 1.0f + z_mid * z_mid;
            }

            /* Compute geodesic distance for adaptive interpolation */
            float diff[DIFFGEO_MAX_DIM];
            for (uint32_t i = 0; i < dim; i++) {
                diff[i] = z2_data[i] - z1_data[i];
            }
            float riem_dist = riemannian_norm(fisher, diff);

            if (isfinite(riem_dist) && riem_dist > DIFFGEO_EPSILON) {
                /* Geodesic interpolation: weight components by inverse metric diagonal
                 * (directions with high curvature get smaller steps) */
                if (riemannian_metric_invert(fisher) == DIFFGEO_OK) {
                    for (uint32_t i = 0; i < dim; i++) {
                        /* Curvature-adaptive alpha per dimension */
                        float g_inv_ii = fisher->g_inv[i * dim + i];
                        float curvature_scale = sqrtf(g_inv_ii);
                        /* Smooth between linear and curvature-scaled alpha */
                        float adj_alpha = alpha * (0.5f + 0.5f * curvature_scale);
                        adj_alpha = nimcp_clampf(adj_alpha, 0.0f, 1.0f);
                        interp_data[i] = (1.0f - adj_alpha) * z1_data[i] + adj_alpha * z2_data[i];
                    }
                    riemannian_metric_destroy(fisher);
                    return 0;
                }
            }
            riemannian_metric_destroy(fisher);
        }
    }

    /* Fallback: standard SLERP on hypersphere */
    float dot = dot_product(z1_data, z2_data, dim);
    float norm1 = vector_norm(z1_data, dim);
    float norm2 = vector_norm(z2_data, dim);

    /* Normalize dot product */
    float cos_omega = dot / (norm1 * norm2 + 1e-8f);
    cos_omega = nimcp_clampf(cos_omega, -1.0f, 1.0f);

    float omega = acosf(cos_omega);

    /* If vectors are nearly parallel, fall back to lerp */
    if (fabsf(omega) < 1e-6f) {
        return vae_latent_lerp(z1, z2, alpha, z_interp);
    }

    float sin_omega = sinf(omega);
    float scale1 = sinf((1.0f - alpha) * omega) / sin_omega;
    float scale2 = sinf(alpha * omega) / sin_omega;

    for (uint32_t i = 0; i < dim; i++) {
        interp_data[i] = scale1 * z1_data[i] + scale2 * z2_data[i];
    }

    return 0;
}

int vae_latent_interpolate_path(const nimcp_tensor_t* z1,
                                const nimcp_tensor_t* z2,
                                uint32_t num_steps,
                                bool use_slerp,
                                nimcp_tensor_t* path) {
    if (!z1 || !z2 || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_latent_interpolate_path");
        return -1;
    }

    if (num_steps < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_CONFIG,
                              "Need at least 2 steps for interpolation");
        return -1;
    }

    vae_latent_heartbeat("vae_latent_interpolate_path", 0.0f);

    uint32_t latent_dim = (z1->shape.rank > 1) ? z1->shape.dims[1] : z1->shape.dims[0];
    float* path_data = (float*)path->data;

    /* Create temporary tensor for each interpolation step */
    uint32_t step_dims[1] = {latent_dim};
    nimcp_tensor_t* step_tensor = nimcp_tensor_create(step_dims, 1, NIMCP_DTYPE_F32);
    if (!step_tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate step tensor");
        return -1;
    }

    for (uint32_t s = 0; s < num_steps; s++) {
        float alpha = (num_steps > 1) ? (float)s / (float)(num_steps - 1) : 0.0f;

        int result = 0;
        if (use_slerp) {
            result = vae_latent_slerp(z1, z2, alpha, step_tensor);
        } else {
            result = vae_latent_lerp(z1, z2, alpha, step_tensor);
        }

        if (result != 0) {
            nimcp_tensor_destroy(step_tensor);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_latent_interpolate_path: interpolation step failed");
            return -1;
        }

        /* Copy to path */
        memcpy(path_data + s * latent_dim, step_tensor->data,
               latent_dim * sizeof(float));

        if ((s & 0xF) == 0) {
            vae_latent_heartbeat("vae_latent_interpolate_path",
                                 (float)s / (float)num_steps);
        }
    }

    nimcp_tensor_destroy(step_tensor);

    vae_latent_heartbeat("vae_latent_interpolate_path", 1.0f);

    return 0;
}

/* ============================================================================
 * Latent State Management Implementation
 * ============================================================================ */

int vae_latent_state_init(vae_latent_state_t* state, uint32_t latent_dim) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL state in vae_latent_state_init");
        return -1;
    }

    if (latent_dim == 0 || latent_dim > VAE_MAX_LATENT_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_DIM,
                              "Invalid latent_dim in vae_latent_state_init");
        return -1;
    }

    memset(state, 0, sizeof(vae_latent_state_t));

    state->latent_dim = latent_dim;

    state->mu = (float*)nimcp_calloc(latent_dim, sizeof(float));
    if (!state->mu) return -1;
    state->log_var = (float*)nimcp_calloc(latent_dim, sizeof(float));
    if (!state->log_var) return -1;
    state->z = (float*)nimcp_calloc(latent_dim, sizeof(float));
    if (!state->z) return -1;
    state->precision = (float*)nimcp_calloc(latent_dim, sizeof(float));

    if (!state->mu || !state->log_var || !state->z || !state->precision) {
        vae_latent_state_free(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate latent state buffers");
        return -1;
    }

    /* Initialize precision to 1 (variance = 1) */
    for (uint32_t i = 0; i < latent_dim; i++) {
        state->precision[i] = 1.0f;
    }

    state->is_valid = false;

    return 0;
}

void vae_latent_state_free(vae_latent_state_t* state) {
    if (!state) return;

    nimcp_free(state->mu);
    nimcp_free(state->log_var);
    nimcp_free(state->z);
    nimcp_free(state->precision);

    memset(state, 0, sizeof(vae_latent_state_t));
}

int vae_latent_state_update(vae_latent_state_t* state,
                            const nimcp_tensor_t* mu,
                            const nimcp_tensor_t* log_var,
                            const nimcp_tensor_t* z) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL state in vae_latent_state_update");
        return -1;
    }

    if (!state->mu || !state->log_var || !state->z) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NOT_INITIALIZED,
                              "Latent state not initialized");
        return -1;
    }

    uint32_t latent_dim = state->latent_dim;

    /* Copy data from tensors (assuming first sample in batch) */
    if (mu) {
        float* mu_data = (float*)mu->data;
        memcpy(state->mu, mu_data, latent_dim * sizeof(float));
    }

    if (log_var) {
        float* logvar_data = (float*)log_var->data;
        memcpy(state->log_var, logvar_data, latent_dim * sizeof(float));

        /* Update precision */
        for (uint32_t i = 0; i < latent_dim; i++) {
            float lv = nimcp_clampf(state->log_var[i],
                               VAE_LATENT_MIN_LOG_VAR,
                               VAE_LATENT_MAX_LOG_VAR);
            state->precision[i] = expf(-lv);
        }
    }

    if (z) {
        float* z_data = (float*)z->data;
        memcpy(state->z, z_data, latent_dim * sizeof(float));
    }

    state->is_valid = (mu != NULL && log_var != NULL && z != NULL);

    return 0;
}

/* ============================================================================
 * Additional State Management
 * ============================================================================ */

vae_latent_state_t* vae_latent_state_create(uint32_t latent_dim) {
    vae_latent_state_t* state = (vae_latent_state_t*)nimcp_calloc(1, sizeof(vae_latent_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate latent state");
        return NULL;
    }

    if (vae_latent_state_init(state, latent_dim) != 0) {
        nimcp_free(state);
        state = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "vae_latent_state_create: validation failed");
        return NULL;
    }

    return state;
}

void vae_latent_state_destroy(vae_latent_state_t* state) {
    if (!state) return;
    vae_latent_state_free(state);
    nimcp_free(state);
    state = NULL;
}

int vae_latent_state_copy(vae_latent_state_t* dst, const vae_latent_state_t* src) {
    if (!dst || !src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL state in vae_latent_state_copy");
        return -1;
    }

    if (dst->latent_dim != src->latent_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_DIM,
                              "Latent dimension mismatch in copy");
        return -1;
    }

    uint32_t dim = src->latent_dim;

    memcpy(dst->mu, src->mu, dim * sizeof(float));
    memcpy(dst->log_var, src->log_var, dim * sizeof(float));
    memcpy(dst->z, src->z, dim * sizeof(float));
    memcpy(dst->precision, src->precision, dim * sizeof(float));
    dst->is_valid = src->is_valid;

    return 0;
}

int vae_latent_state_reset(vae_latent_state_t* state) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL state in vae_latent_state_reset");
        return -1;
    }

    uint32_t dim = state->latent_dim;

    memset(state->mu, 0, dim * sizeof(float));
    memset(state->log_var, 0, dim * sizeof(float));
    memset(state->z, 0, dim * sizeof(float));

    for (uint32_t i = 0; i < dim; i++) {
        state->precision[i] = 1.0f;
    }

    state->is_valid = false;

    return 0;
}
