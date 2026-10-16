/**
 * @file nimcp_information_geometry.c
 * @brief Information Geometry Module Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "physics/geometry/nimcp_information_geometry.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/geometry/nimcp_differential_geometry.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(information_geometry)

//=============================================================================
// Internal Structures
//=============================================================================

struct nimcp_info_geometry_struct {
    nimcp_info_geom_config_t config;
    nimcp_brain_t brain;

    /* Fisher matrix and inverse */
    float* fisher_matrix;
    float* fisher_inverse;
    uint32_t fisher_dim;

    /* Embedding state */
    float embedding[INFO_GEOM_MAX_LATENT_DIM];
    float tangent_vectors[INFO_GEOM_MAX_LATENT_DIM * INFO_GEOM_MAX_AMBIENT_DIM];

    /* EMA of Fisher matrix */
    float* fisher_ema;

    /* State */
    nimcp_info_geom_state_t state;
    nimcp_info_geom_stats_t stats;
    bool is_initialized;
};

struct nimcp_fisher_info_struct {
    nimcp_fisher_config_t config;
    float* matrix;
    float* inverse;
    float* gradient_accum;
    uint32_t dim;
    uint32_t samples_accumulated;
    float damping;
    bool inverse_valid;
};

struct nimcp_natural_gradient_struct {
    nimcp_natural_grad_config_t config;
    nimcp_fisher_info_t fisher;
    float* momentum_buffer;
    uint32_t param_dim;
    uint32_t step_count;
    float current_lr;
};

struct nimcp_neural_manifold_struct {
    nimcp_manifold_config_t config;
    float* samples;
    uint32_t num_samples;
    uint32_t max_samples;
    uint32_t ambient_dim;
    uint32_t estimated_dim;
    float* local_pca;
    bool dim_estimated;
};

//=============================================================================
// Helper Functions
//=============================================================================

static void matrix_multiply(const float* A, const float* B, float* C,
                           uint32_t m, uint32_t n, uint32_t k) {
    for (uint32_t i = 0; i < m; i++) {
        for (uint32_t j = 0; j < k; j++) {
            float sum = 0.0f;
            for (uint32_t l = 0; l < n; l++) {
                sum += A[i * n + l] * B[l * k + j];
            }
            C[i * k + j] = sum;
        }
    }
}

static void matrix_vector_multiply(const float* A, const float* x, float* y,
                                   uint32_t m, uint32_t n) {
    for (uint32_t i = 0; i < m; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            sum += A[i * n + j] * x[j];
        }
        y[i] = sum;
    }
}

static bool invert_matrix_cholesky(const float* A, float* A_inv, uint32_t n, float reg) {
    /* Cholesky decomposition with regularization: A = L * L^T */
    float* L = (float*)nimcp_calloc(n * n, sizeof(float));
    if (!L) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "invert_matrix_cholesky: L is NULL");
        return false;
    }

    /* Add regularization to diagonal */
    float* A_reg = (float*)nimcp_malloc(n * n * sizeof(float));
    if (!A_reg) { nimcp_free(L); return false; }

    memcpy(A_reg, A, n * n * sizeof(float));
    for (uint32_t i = 0; i < n; i++) {
        A_reg[i * n + i] += reg;
    }

    /* Cholesky decomposition */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            float sum = A_reg[i * n + j];
            for (uint32_t k = 0; k < j; k++) {
                sum -= L[i * n + k] * L[j * n + k];
            }
            if (i == j) {
                if (sum <= 0.0f) {
                    nimcp_free(L);
                    nimcp_free(A_reg);
                    return false;  /* Not positive definite */
                }
                L[i * n + j] = sqrtf(sum);
            } else {
                L[i * n + j] = sum / L[j * n + j];
            }
        }
    }

    /* Compute L^{-1} */
    float* L_inv = (float*)nimcp_calloc(n * n, sizeof(float));
    if (!L_inv) { nimcp_free(L); nimcp_free(A_reg); return false; }

    for (uint32_t i = 0; i < n; i++) {
        L_inv[i * n + i] = 1.0f / L[i * n + i];
        for (uint32_t j = i + 1; j < n; j++) {
            float sum = 0.0f;
            for (uint32_t k = i; k < j; k++) {
                sum -= L[j * n + k] * L_inv[k * n + i];
            }
            L_inv[j * n + i] = sum / L[j * n + j];
        }
    }

    /* A^{-1} = L^{-T} * L^{-1} */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < n; k++) {
                sum += L_inv[k * n + i] * L_inv[k * n + j];
            }
            A_inv[i * n + j] = sum;
        }
    }

    nimcp_free(L);
    nimcp_free(L_inv);
    nimcp_free(A_reg);
    return true;
}

static float vector_norm(const float* v, uint32_t n) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += v[i] * v[i];
    }
    return sqrtf(sum);
}

static void clip_vector(float* v, uint32_t n, float max_norm) {
    float norm = vector_norm(v, n);
    if (norm > max_norm) {
        float scale = max_norm / norm;
        for (uint32_t i = 0; i < n; i++) {
            v[i] *= scale;
        }
    }
}

//=============================================================================
// Information Geometry Implementation
//=============================================================================

nimcp_info_geom_config_t nimcp_info_geom_default_config(void) {
    nimcp_info_geom_config_t config = {
        .latent_dim = INFO_GEOM_DEFAULT_LATENT_DIM,
        .ambient_dim = 256,
        .regularization = INFO_GEOM_REGULARIZATION,
        .learning_rate = NIMCP_LEARNING_RATE_DEFAULT,
        .gradient_clip = INFO_GEOM_GRAD_CLIP,
        .enable_ema = true,
        .ema_decay = NIMCP_EMA_DECAY_DEFAULT,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_info_geometry_t nimcp_info_geom_create(const nimcp_info_geom_config_t* config) {
    nimcp_info_geometry_t geom = (nimcp_info_geometry_t)nimcp_calloc(1, sizeof(struct nimcp_info_geometry_struct));
    if (!geom) {
        LOG_ERROR("Failed to allocate information geometry structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate info geometry");
        return NULL;
    }

    geom->config = config ? *config : nimcp_info_geom_default_config();

    /* Validate dimensions */
    if (geom->config.latent_dim > INFO_GEOM_MAX_LATENT_DIM) {
        geom->config.latent_dim = INFO_GEOM_MAX_LATENT_DIM;
    }
    if (geom->config.ambient_dim > INFO_GEOM_MAX_AMBIENT_DIM) {
        geom->config.ambient_dim = INFO_GEOM_MAX_AMBIENT_DIM;
    }

    uint32_t dim = geom->config.latent_dim;
    geom->fisher_dim = dim;

    /* Allocate Fisher matrices */
    geom->fisher_matrix = (float*)nimcp_calloc(dim * dim, sizeof(float));
    geom->fisher_inverse = (float*)nimcp_calloc(dim * dim, sizeof(float));

    if (!geom->fisher_matrix || !geom->fisher_inverse) {
        LOG_ERROR("Failed to allocate Fisher matrices");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate Fisher matrices");
        nimcp_info_geom_destroy(geom);
        return NULL;
    }

    /* Initialize Fisher as identity */
    for (uint32_t i = 0; i < dim; i++) {
        geom->fisher_matrix[i * dim + i] = 1.0f;
        geom->fisher_inverse[i * dim + i] = 1.0f;
    }

    if (geom->config.enable_ema) {
        geom->fisher_ema = (float*)nimcp_calloc(dim * dim, sizeof(float));
        if (!geom->fisher_ema) {
            LOG_ERROR("Failed to allocate Fisher EMA buffer");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate Fisher EMA");
            nimcp_info_geom_destroy(geom);
            return NULL;
        }
        memcpy(geom->fisher_ema, geom->fisher_matrix, dim * dim * sizeof(float));
    }

    return geom;
}

void nimcp_info_geom_destroy(nimcp_info_geometry_t geom) {
    if (!geom) return;
    nimcp_free(geom->fisher_matrix);
    nimcp_free(geom->fisher_inverse);
    nimcp_free(geom->fisher_ema);
    nimcp_free(geom);
}

nimcp_info_geom_error_t nimcp_info_geom_init(nimcp_info_geometry_t geom, nimcp_brain_t brain) {
    if (!geom) return INFO_GEOM_ERR_NULL_PTR;
    if (geom->is_initialized) return INFO_GEOM_ERR_ALREADY_INITIALIZED;

    geom->brain = brain;
    geom->is_initialized = true;
    geom->state.is_initialized = true;

    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_info_geom_shutdown(nimcp_info_geometry_t geom) {
    if (!geom) return INFO_GEOM_ERR_NULL_PTR;
    if (!geom->is_initialized) return INFO_GEOM_ERR_NOT_INITIALIZED;

    geom->is_initialized = false;
    geom->state.is_initialized = false;

    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_info_geom_compute_fisher(
    nimcp_info_geometry_t geom,
    const float* distribution,
    uint32_t dist_size
) {
    if (!geom || !distribution) return INFO_GEOM_ERR_NULL_PTR;

    uint32_t dim = geom->fisher_dim;
    if (dist_size != dim) return INFO_GEOM_ERR_INVALID_DIM;

    /* Compute Fisher information: F_ij = E[d log p / d theta_i * d log p / d theta_j]
     * For categorical distribution: F_ij = sum_k (1/p_k) * dp_k/dtheta_i * dp_k/dtheta_j
     * Simplified: F = diag(1/p) for softmax parameterization */

    float* F = geom->fisher_matrix;
    memset(F, 0, dim * dim * sizeof(float));

    /* Diagonal Fisher for softmax */
    for (uint32_t i = 0; i < dim; i++) {
        float p = distribution[i];
        if (p > 1e-8f) {
            F[i * dim + i] = 1.0f / p;
        } else {
            F[i * dim + i] = 1e8f;  /* Large but finite */
        }
    }

    /* Apply EMA if enabled */
    if (geom->config.enable_ema && geom->fisher_ema) {
        float decay = geom->config.ema_decay;
        for (uint32_t i = 0; i < dim * dim; i++) {
            geom->fisher_ema[i] = decay * geom->fisher_ema[i] + (1.0f - decay) * F[i];
        }
        memcpy(F, geom->fisher_ema, dim * dim * sizeof(float));
    }

    /* Compute inverse */
    if (!invert_matrix_cholesky(F, geom->fisher_inverse, dim, geom->config.regularization)) {
        return INFO_GEOM_ERR_SINGULAR_MATRIX;
    }

    geom->stats.fisher_computations++;

    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_info_geom_natural_gradient(
    nimcp_info_geometry_t geom,
    const float* gradient,
    float* natural_grad,
    uint32_t grad_size
) {
    if (!geom || !gradient || !natural_grad) return INFO_GEOM_ERR_NULL_PTR;
    if (grad_size != geom->fisher_dim) return INFO_GEOM_ERR_INVALID_DIM;

    /* Natural gradient = F^{-1} * gradient */
    matrix_vector_multiply(geom->fisher_inverse, gradient, natural_grad,
                          geom->fisher_dim, geom->fisher_dim);

    /* Clip if needed */
    clip_vector(natural_grad, grad_size, geom->config.gradient_clip);

    /* Track speedup */
    float grad_norm = vector_norm(gradient, grad_size);
    float nat_grad_norm = vector_norm(natural_grad, grad_size);

    geom->stats.avg_gradient_norm = geom->stats.avg_gradient_norm * 0.99f + grad_norm * 0.01f;
    if (!isfinite(geom->stats.avg_gradient_norm)) geom->stats.avg_gradient_norm = grad_norm;
    geom->stats.avg_natural_grad_norm = geom->stats.avg_natural_grad_norm * 0.99f + nat_grad_norm * 0.01f;
    if (!isfinite(geom->stats.avg_natural_grad_norm)) geom->stats.avg_natural_grad_norm = nat_grad_norm;

    if (grad_norm > 1e-8f) {
        float ratio = nat_grad_norm / grad_norm;
        geom->stats.avg_speedup_ratio = geom->stats.avg_speedup_ratio * 0.99f + ratio * 0.01f;
        if (!isfinite(geom->stats.avg_speedup_ratio)) geom->stats.avg_speedup_ratio = ratio;
    }

    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_info_geom_update(
    nimcp_info_geometry_t geom,
    float* parameters,
    const float* gradient,
    uint32_t param_size,
    float learning_rate
) {
    if (!geom || !parameters || !gradient) return INFO_GEOM_ERR_NULL_PTR;

    float* natural_grad = (float*)nimcp_malloc(param_size * sizeof(float));
    if (!natural_grad) return INFO_GEOM_ERR_NO_MEMORY;

    nimcp_info_geom_error_t err = nimcp_info_geom_natural_gradient(
        geom, gradient, natural_grad, param_size);

    if (err == INFO_GEOM_OK) {
        /* Update parameters: theta = theta - lr * natural_grad */
        for (uint32_t i = 0; i < param_size; i++) {
            parameters[i] -= learning_rate * natural_grad[i];
        }
        geom->stats.updates++;
        geom->state.update_count++;
    }

    nimcp_free(natural_grad);
    return err;
}

nimcp_info_geom_error_t nimcp_info_geom_geodesic_distance(
    nimcp_info_geometry_t geom,
    const float* point_a,
    const float* point_b,
    uint32_t dim,
    float* distance
) {
    if (!geom || !point_a || !point_b || !distance) return INFO_GEOM_ERR_NULL_PTR;

    /* Use the Riemannian metric module to compute proper geodesic distance
     * on the Fisher-Rao statistical manifold. The Fisher information matrix
     * serves as the metric tensor g_{ij}. */

    if (dim <= DIFFGEO_MAX_DIM && geom->fisher_matrix) {
        riemannian_metric_t* metric = riemannian_metric_create(dim);
        if (metric) {
            riemannian_metric_set(metric, geom->fisher_matrix);
            riemannian_metric_invert(metric);

            /* For nearby points, the infinitesimal approximation
             * d(p,q) ≈ ||q-p||_g is sufficient. For distant points,
             * we'd need the full geodesic equation. Use the Riemannian
             * norm of the displacement as a first-order approximation. */
            float delta[DIFFGEO_MAX_DIM];
            for (uint32_t i = 0; i < dim; i++) {
                delta[i] = point_b[i] - point_a[i];
            }

            *distance = riemannian_norm(metric, delta);
            geom->state.geodesic_distance = *distance;
            riemannian_metric_destroy(metric);
            return INFO_GEOM_OK;
        }
    }

    /* Fallback: direct Fisher-Rao computation */
    float* delta = (float*)nimcp_malloc(dim * sizeof(float));
    float* F_delta = (float*)nimcp_malloc(dim * sizeof(float));
    if (!delta || !F_delta) {
        nimcp_free(delta);
        nimcp_free(F_delta);
        return INFO_GEOM_ERR_NO_MEMORY;
    }

    for (uint32_t i = 0; i < dim; i++) {
        delta[i] = point_b[i] - point_a[i];
    }

    matrix_vector_multiply(geom->fisher_matrix, delta, F_delta, dim, dim);

    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum += delta[i] * F_delta[i];
    }

    *distance = sqrtf(fmaxf(0.0f, sum));
    geom->state.geodesic_distance = *distance;

    nimcp_free(delta);
    nimcp_free(F_delta);

    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_info_geom_kl_divergence(
    nimcp_info_geometry_t geom,
    const float* p,
    const float* q,
    uint32_t size,
    float* kl_div
) {
    if (!geom || !p || !q || !kl_div) return INFO_GEOM_ERR_NULL_PTR;

    /* KL(p || q) = sum_i p_i * log(p_i / q_i) */
    float kl = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        if (p[i] > 1e-10f && q[i] > 1e-10f) {
            kl += p[i] * logf(p[i] / q[i]);
        }
    }

    *kl_div = kl;
    geom->state.kl_divergence = kl;

    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_info_geom_get_state(
    nimcp_info_geometry_t geom,
    nimcp_info_geom_state_t* state
) {
    if (!geom || !state) return INFO_GEOM_ERR_NULL_PTR;
    *state = geom->state;
    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_info_geom_get_stats(
    nimcp_info_geometry_t geom,
    nimcp_info_geom_stats_t* stats
) {
    if (!geom || !stats) return INFO_GEOM_ERR_NULL_PTR;
    *stats = geom->stats;
    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_info_geom_reset_stats(nimcp_info_geometry_t geom) {
    if (!geom) return INFO_GEOM_ERR_NULL_PTR;
    memset(&geom->stats, 0, sizeof(geom->stats));
    return INFO_GEOM_OK;
}

//=============================================================================
// Fisher Information Implementation
//=============================================================================

nimcp_fisher_config_t nimcp_fisher_default_config(void) {
    nimcp_fisher_config_t config = {
        .param_dim = 64,
        .sample_size = 32,
        .regularization = 1e-4f,
        .use_empirical = true,
        .enable_damping = true,
        .initial_damping = 0.1f
    };
    return config;
}

nimcp_fisher_info_t nimcp_fisher_create(const nimcp_fisher_config_t* config) {
    nimcp_fisher_info_t fisher = (nimcp_fisher_info_t)nimcp_calloc(1, sizeof(struct nimcp_fisher_info_struct));
    if (!fisher) {
        LOG_ERROR("Failed to allocate Fisher info structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate Fisher info");
        return NULL;
    }

    fisher->config = config ? *config : nimcp_fisher_default_config();
    fisher->dim = fisher->config.param_dim;
    fisher->damping = fisher->config.initial_damping;

    fisher->matrix = (float*)nimcp_calloc(fisher->dim * fisher->dim, sizeof(float));
    fisher->inverse = (float*)nimcp_calloc(fisher->dim * fisher->dim, sizeof(float));
    fisher->gradient_accum = (float*)nimcp_calloc(fisher->dim * fisher->dim, sizeof(float));

    if (!fisher->matrix || !fisher->inverse || !fisher->gradient_accum) {
        LOG_ERROR("Failed to allocate Fisher matrices or gradient buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate Fisher buffers");
        nimcp_fisher_destroy(fisher);
        return NULL;
    }

    /* Initialize as identity */
    for (uint32_t i = 0; i < fisher->dim; i++) {
        fisher->matrix[i * fisher->dim + i] = 1.0f;
        fisher->inverse[i * fisher->dim + i] = 1.0f;
    }

    return fisher;
}

void nimcp_fisher_destroy(nimcp_fisher_info_t fisher) {
    if (!fisher) return;
    nimcp_free(fisher->matrix);
    nimcp_free(fisher->inverse);
    nimcp_free(fisher->gradient_accum);
    nimcp_free(fisher);
}

nimcp_info_geom_error_t nimcp_fisher_compute(
    nimcp_fisher_info_t fisher,
    const float* gradients,
    uint32_t num_samples,
    uint32_t grad_dim
) {
    if (!fisher || !gradients) return INFO_GEOM_ERR_NULL_PTR;
    if (grad_dim != fisher->dim) return INFO_GEOM_ERR_INVALID_DIM;

    /* Empirical Fisher: F = (1/n) * sum_i g_i * g_i^T */
    memset(fisher->matrix, 0, fisher->dim * fisher->dim * sizeof(float));

    for (uint32_t s = 0; s < num_samples; s++) {
        const float* g = &gradients[s * grad_dim];
        for (uint32_t i = 0; i < fisher->dim; i++) {
            for (uint32_t j = 0; j < fisher->dim; j++) {
                fisher->matrix[i * fisher->dim + j] += g[i] * g[j];
            }
        }
    }

    /* Average */
    float scale = 1.0f / (float)num_samples;
    for (uint32_t i = 0; i < fisher->dim * fisher->dim; i++) {
        fisher->matrix[i] *= scale;
    }

    /* Add damping */
    if (fisher->config.enable_damping) {
        for (uint32_t i = 0; i < fisher->dim; i++) {
            fisher->matrix[i * fisher->dim + i] += fisher->damping;
        }
    }

    /* Compute inverse */
    fisher->inverse_valid = invert_matrix_cholesky(
        fisher->matrix, fisher->inverse, fisher->dim, fisher->config.regularization);

    return fisher->inverse_valid ? INFO_GEOM_OK : INFO_GEOM_ERR_SINGULAR_MATRIX;
}

nimcp_info_geom_error_t nimcp_fisher_compute_empirical(
    nimcp_fisher_info_t fisher,
    const float* samples,
    const float* log_probs,
    uint32_t num_samples,
    uint32_t sample_dim
) {
    (void)samples;
    (void)log_probs;
    (void)num_samples;
    (void)sample_dim;

    /* Placeholder - compute Fisher from log probability gradients */
    if (!fisher) return INFO_GEOM_ERR_NULL_PTR;
    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_fisher_get_matrix(
    nimcp_fisher_info_t fisher,
    float* matrix,
    uint32_t size,
    bool get_inverse
) {
    if (!fisher || !matrix) return INFO_GEOM_ERR_NULL_PTR;
    if (size != fisher->dim * fisher->dim) return INFO_GEOM_ERR_INVALID_DIM;

    if (get_inverse) {
        if (!fisher->inverse_valid) return INFO_GEOM_ERR_SINGULAR_MATRIX;
        memcpy(matrix, fisher->inverse, size * sizeof(float));
    } else {
        memcpy(matrix, fisher->matrix, size * sizeof(float));
    }

    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_fisher_solve(
    nimcp_fisher_info_t fisher,
    const float* b,
    float* x,
    uint32_t size
) {
    if (!fisher || !b || !x) return INFO_GEOM_ERR_NULL_PTR;
    if (size != fisher->dim) return INFO_GEOM_ERR_INVALID_DIM;
    if (!fisher->inverse_valid) return INFO_GEOM_ERR_SINGULAR_MATRIX;

    matrix_vector_multiply(fisher->inverse, b, x, fisher->dim, fisher->dim);

    return INFO_GEOM_OK;
}

//=============================================================================
// Natural Gradient Implementation
//=============================================================================

nimcp_natural_grad_config_t nimcp_natural_grad_default_config(void) {
    nimcp_natural_grad_config_t config = {
        .learning_rate = NIMCP_LEARNING_RATE_DEFAULT,
        .momentum = NIMCP_MOMENTUM_DEFAULT,
        .gradient_clip = 10.0f,
        .use_preconditioner = true,
        .enable_warmup = true,
        .warmup_steps = 100
    };
    return config;
}

nimcp_natural_gradient_t nimcp_natural_grad_create(
    const nimcp_natural_grad_config_t* config,
    uint32_t param_dim
) {
    nimcp_natural_gradient_t ng = (nimcp_natural_gradient_t)nimcp_calloc(1, sizeof(struct nimcp_natural_gradient_struct));
    if (!ng) {
        LOG_ERROR("Failed to allocate natural gradient structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate natural gradient");
        return NULL;
    }

    ng->config = config ? *config : nimcp_natural_grad_default_config();
    ng->param_dim = param_dim;
    ng->current_lr = ng->config.enable_warmup ? 0.0f : ng->config.learning_rate;

    ng->momentum_buffer = (float*)nimcp_calloc(param_dim, sizeof(float));
    if (!ng->momentum_buffer) {
        LOG_ERROR("Failed to allocate momentum buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate momentum buffer");
        nimcp_free(ng);
        return NULL;
    }

    return ng;
}

void nimcp_natural_grad_destroy(nimcp_natural_gradient_t ng) {
    if (!ng) return;
    nimcp_free(ng->momentum_buffer);
    nimcp_free(ng);
}

nimcp_info_geom_error_t nimcp_natural_grad_update_fisher(
    nimcp_natural_gradient_t ng,
    nimcp_fisher_info_t fisher
) {
    if (!ng) return INFO_GEOM_ERR_NULL_PTR;
    ng->fisher = fisher;
    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_natural_grad_step(
    nimcp_natural_gradient_t ng,
    float* parameters,
    const float* gradient,
    uint32_t size
) {
    if (!ng || !parameters || !gradient) return INFO_GEOM_ERR_NULL_PTR;
    if (size != ng->param_dim) return INFO_GEOM_ERR_INVALID_DIM;

    /* Update warmup */
    if (ng->config.enable_warmup && ng->step_count < ng->config.warmup_steps) {
        float warmup_factor = (float)(ng->step_count + 1) / (float)ng->config.warmup_steps;
        ng->current_lr = ng->config.learning_rate * warmup_factor;
    } else {
        ng->current_lr = ng->config.learning_rate;
    }

    float* update = (float*)nimcp_malloc(size * sizeof(float));
    if (!update) return INFO_GEOM_ERR_NO_MEMORY;

    /* Compute natural gradient if Fisher available */
    if (ng->fisher && ng->config.use_preconditioner) {
        nimcp_info_geom_error_t err = nimcp_fisher_solve(ng->fisher, gradient, update, size);
        if (err != INFO_GEOM_OK) {
            /* Fall back to standard gradient */
            memcpy(update, gradient, size * sizeof(float));
        }
    } else {
        memcpy(update, gradient, size * sizeof(float));
    }

    /* Clip gradient */
    clip_vector(update, size, ng->config.gradient_clip);

    /* Apply momentum */
    for (uint32_t i = 0; i < size; i++) {
        ng->momentum_buffer[i] = ng->config.momentum * ng->momentum_buffer[i] + update[i];
        parameters[i] -= ng->current_lr * ng->momentum_buffer[i];
    }

    ng->step_count++;

    nimcp_free(update);
    return INFO_GEOM_OK;
}

float nimcp_natural_grad_get_lr(nimcp_natural_gradient_t ng) {
    return ng ? ng->current_lr : 0.0f;
}

//=============================================================================
// Neural Manifold Implementation
//=============================================================================

nimcp_manifold_config_t nimcp_manifold_default_config(void) {
    nimcp_manifold_config_t config = {
        .intrinsic_dim = 0,  /* Estimate automatically */
        .num_samples = 1000,
        .neighborhood_radius = 0.1f,
        .compute_curvature = true,
        .enable_embedding = false
    };
    return config;
}

nimcp_neural_manifold_t nimcp_manifold_create(
    const nimcp_manifold_config_t* config,
    uint32_t ambient_dim
) {
    nimcp_neural_manifold_t manifold = (nimcp_neural_manifold_t)nimcp_calloc(1, sizeof(struct nimcp_neural_manifold_struct));
    if (!manifold) {
        LOG_ERROR("Failed to allocate neural manifold structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate manifold");
        return NULL;
    }

    manifold->config = config ? *config : nimcp_manifold_default_config();
    manifold->ambient_dim = ambient_dim;
    manifold->max_samples = manifold->config.num_samples;

    manifold->samples = (float*)nimcp_calloc(manifold->max_samples * ambient_dim, sizeof(float));
    if (!manifold->samples) {
        LOG_ERROR("Failed to allocate manifold samples buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate manifold samples");
        nimcp_free(manifold);
        return NULL;
    }

    return manifold;
}

void nimcp_manifold_destroy(nimcp_neural_manifold_t manifold) {
    if (!manifold) return;
    nimcp_free(manifold->samples);
    nimcp_free(manifold->local_pca);
    nimcp_free(manifold);
}

nimcp_info_geom_error_t nimcp_manifold_add_samples(
    nimcp_neural_manifold_t manifold,
    const float* samples,
    uint32_t num_samples,
    uint32_t sample_dim
) {
    if (!manifold || !samples) return INFO_GEOM_ERR_NULL_PTR;
    if (sample_dim != manifold->ambient_dim) return INFO_GEOM_ERR_INVALID_DIM;

    uint32_t to_add = num_samples;
    if (manifold->num_samples + to_add > manifold->max_samples) {
        to_add = manifold->max_samples - manifold->num_samples;
    }

    memcpy(&manifold->samples[manifold->num_samples * manifold->ambient_dim],
           samples, to_add * sample_dim * sizeof(float));
    manifold->num_samples += to_add;

    /* Invalidate dimension estimate */
    manifold->dim_estimated = false;

    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_manifold_estimate_dim(
    nimcp_neural_manifold_t manifold,
    uint32_t* intrinsic_dim
) {
    if (!manifold || !intrinsic_dim) return INFO_GEOM_ERR_NULL_PTR;
    if (manifold->num_samples < 10) return INFO_GEOM_ERR_COMPUTATION;

    /* Use maximum likelihood estimation of intrinsic dimensionality
     * Based on distances to k-nearest neighbors */

    /* Simplified: estimate based on variance ratios from PCA */
    /* In practice, would use MLE or correlation dimension */

    /* For now, return config value or estimate as sqrt(num_samples) */
    if (manifold->config.intrinsic_dim > 0) {
        manifold->estimated_dim = manifold->config.intrinsic_dim;
    } else {
        /* Heuristic estimate */
        manifold->estimated_dim = (uint32_t)sqrtf((float)manifold->num_samples);
        if (manifold->estimated_dim > manifold->ambient_dim) {
            manifold->estimated_dim = manifold->ambient_dim;
        }
        if (manifold->estimated_dim < 1) {
            manifold->estimated_dim = 1;
        }
    }

    manifold->dim_estimated = true;
    *intrinsic_dim = manifold->estimated_dim;

    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_manifold_curvature(
    nimcp_neural_manifold_t manifold,
    const float* point,
    uint32_t dim,
    float* curvature
) {
    if (!manifold || !point || !curvature) return INFO_GEOM_ERR_NULL_PTR;
    if (dim != manifold->ambient_dim) return INFO_GEOM_ERR_INVALID_DIM;

    /* Use local neighborhood to estimate a Riemannian metric, then compute
     * scalar curvature via the differential geometry module's Ricci tensor.
     * Falls back to density-based estimate if insufficient neighbors. */
    float sum_sq_dist = 0.0f;
    uint32_t count = 0;
    float radius = manifold->config.neighborhood_radius;

    /* Accumulate local covariance matrix as metric estimate */
    uint32_t ldim = (dim < DIFFGEO_MAX_DIM) ? dim : DIFFGEO_MAX_DIM;
    float covariance[DIFFGEO_MAX_DIM * DIFFGEO_MAX_DIM];
    memset(covariance, 0, ldim * ldim * sizeof(float));

    for (uint32_t i = 0; i < manifold->num_samples; i++) {
        const float* sample = &manifold->samples[i * dim];
        float dist_sq = 0.0f;
        for (uint32_t j = 0; j < dim; j++) {
            float d = sample[j] - point[j];
            dist_sq += d * d;
        }
        float dist = sqrtf(dist_sq);
        if (dist < radius && dist > 1e-8f) {
            sum_sq_dist += dist_sq;
            count++;
            /* Accumulate outer product for covariance */
            for (uint32_t a = 0; a < ldim; a++) {
                float da = sample[a] - point[a];
                for (uint32_t b = 0; b < ldim; b++) {
                    covariance[a * ldim + b] += da * (sample[b] - point[b]);
                }
            }
        }
    }

    if (count >= ldim + 1 && ldim >= 2) {
        /* Normalize covariance */
        for (uint32_t i = 0; i < ldim * ldim; i++) {
            covariance[i] /= (float)count;
        }

        /* Use covariance as local metric tensor, compute scalar curvature
         * via Ricci contraction. For flat regions, curvature ≈ 0. */
        riemannian_metric_t* metric = riemannian_metric_create(ldim);
        if (metric) {
            riemannian_metric_set(metric, covariance);
            if (riemannian_metric_invert(metric) == DIFFGEO_OK) {
                /* Scalar curvature from Ricci tensor (zero Christoffel derivs
                 * at this point — use density-weighted estimate) */
                curvature_data_t* curv_data = curvature_create(ldim);
                if (curv_data) {
                    curvature_compute_scalar(curv_data, metric);
                    *curvature = curv_data->scalar_curvature;
                    curvature_destroy(curv_data);
                    riemannian_metric_destroy(metric);
                    return INFO_GEOM_OK;
                }
                curvature_destroy(curv_data);
            }
            riemannian_metric_destroy(metric);
        }
    }

    /* Fallback: curvature estimate based on neighborhood density */
    if (count > 0) {
        float avg_dist_sq = sum_sq_dist / (float)count;
        *curvature = 1.0f / (1.0f + avg_dist_sq * (float)count);
    } else {
        *curvature = 0.0f;
    }

    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_manifold_project(
    nimcp_neural_manifold_t manifold,
    const float* point,
    float* projected,
    uint32_t dim
) {
    if (!manifold || !point || !projected) return INFO_GEOM_ERR_NULL_PTR;
    if (dim != manifold->ambient_dim) return INFO_GEOM_ERR_INVALID_DIM;

    /* Project to nearest point on manifold (approximated by samples) */
    float min_dist = INFINITY;
    uint32_t nearest_idx = 0;

    for (uint32_t i = 0; i < manifold->num_samples; i++) {
        const float* sample = &manifold->samples[i * dim];
        float dist_sq = 0.0f;
        for (uint32_t j = 0; j < dim; j++) {
            float d = sample[j] - point[j];
            dist_sq += d * d;
        }
        if (dist_sq < min_dist) {
            min_dist = dist_sq;
            nearest_idx = i;
        }
    }

    memcpy(projected, &manifold->samples[nearest_idx * dim], dim * sizeof(float));

    return INFO_GEOM_OK;
}

nimcp_info_geom_error_t nimcp_manifold_geodesic(
    nimcp_neural_manifold_t manifold,
    const float* start,
    const float* end,
    float* path,
    uint32_t path_steps,
    uint32_t dim
) {
    if (!manifold || !start || !end || !path) return INFO_GEOM_ERR_NULL_PTR;
    if (dim != manifold->ambient_dim) return INFO_GEOM_ERR_INVALID_DIM;
    if (path_steps < 2) return INFO_GEOM_ERR_INVALID_DIM;

    /* Approximate geodesic by linear interpolation projected to manifold */
    for (uint32_t step = 0; step < path_steps; step++) {
        float t = (float)step / (float)(path_steps - 1);
        float* path_point = &path[step * dim];

        /* Linear interpolation */
        for (uint32_t j = 0; j < dim; j++) {
            path_point[j] = start[j] * (1.0f - t) + end[j] * t;
        }

        /* Project to manifold */
        float* projected = (float*)nimcp_malloc(dim * sizeof(float));
        if (projected) {
            nimcp_manifold_project(manifold, path_point, projected, dim);
            memcpy(path_point, projected, dim * sizeof(float));
            nimcp_free(projected);
        }
    }

    return INFO_GEOM_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_info_geom_error_string(nimcp_info_geom_error_t err) {
    switch (err) {
        case INFO_GEOM_OK: return "OK";
        case INFO_GEOM_ERR_NULL_PTR: return "Null pointer";
        case INFO_GEOM_ERR_INVALID_DIM: return "Invalid dimension";
        case INFO_GEOM_ERR_SINGULAR_MATRIX: return "Singular matrix";
        case INFO_GEOM_ERR_NOT_INITIALIZED: return "Not initialized";
        case INFO_GEOM_ERR_ALREADY_INITIALIZED: return "Already initialized";
        case INFO_GEOM_ERR_NO_MEMORY: return "Out of memory";
        case INFO_GEOM_ERR_COMPUTATION: return "Computation error";
        default: return "Unknown error";
    }
}

/*=============================================================================
 * W15: Information-geometry runtime KG emission + read path
 *===========================================================================*/

#define INFO_GEOM_CORE_KG_ROOT_NAME "information_geometry_runtime"

static struct brain_struct* s_info_geom_kg_brain = NULL;

void nimcp_info_geom_kg_register_brain(brain_t brain) {
    s_info_geom_kg_brain = brain;
}

void nimcp_info_geom_kg_trigger_manifold_event(brain_t brain,
                                               const char* kind,
                                               float speedup_ratio,
                                               float ricci_curvature,
                                               uint64_t ts_us) {
    if (!brain) brain = s_info_geom_kg_brain;
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    if (!kind) return;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    brain_kg_node_id_t root = brain_kg_find_node(kg, INFO_GEOM_CORE_KG_ROOT_NAME);
    if (root == BRAIN_KG_INVALID_NODE) {
        root = brain_kg_add_node(kg, INFO_GEOM_CORE_KG_ROOT_NAME,
            BRAIN_KG_NODE_CORE,
            "Information geometry runtime aggregator - manifold tracking events");
        /* Link to either bridge root or create edge to "information_geometry". */
        brain_kg_node_id_t peer = brain_kg_find_node(kg, "information_geometry");
        if (root != BRAIN_KG_INVALID_NODE && peer != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, peer, root,
                BRAIN_KG_EDGE_INTEGRATES_WITH,
                "runtime aggregator for info-geom module", 1.0f);
        }
    }

    char ev_name[160];
    snprintf(ev_name, sizeof(ev_name),
             "information_geometry_runtime_event_%s_%llu",
             kind, (unsigned long long)ts_us);
    char desc[240];
    snprintf(desc, sizeof(desc),
             "Info-geom runtime event: kind=%s speedup=%.4f curvature=%.4f",
             kind, speedup_ratio,
             isnan(ricci_curvature) ? 0.0f : ricci_curvature);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, ev_name,
        BRAIN_KG_NODE_CORE, desc);
    if (ev != BRAIN_KG_INVALID_NODE && root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root, ev, BRAIN_KG_EDGE_SENDS_TO,
            "manifold_event",
            speedup_ratio > 0.0f ? speedup_ratio : 1.0f);
    }
    if (ev != BRAIN_KG_INVALID_NODE) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%.6f", speedup_ratio);
        brain_kg_add_metadata(kg, ev, "speedup_ratio", buf);
        if (!isnan(ricci_curvature)) {
            snprintf(buf, sizeof(buf), "%.6f", ricci_curvature);
            brain_kg_add_metadata(kg, ev, "ricci_curvature", buf);
        }
        brain_kg_add_metadata(kg, ev, "kind", kind);
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", buf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

uint32_t nimcp_info_geom_kg_count_events(struct brain_kg* kg_opaque,
                                         const char* kind_substr) {
    brain_kg_t* kg = (brain_kg_t*)kg_opaque;
    if (!kg) return 0;
    brain_kg_node_id_t root = brain_kg_find_node(kg, INFO_GEOM_CORE_KG_ROOT_NAME);
    if (root == BRAIN_KG_INVALID_NODE) return 0;
    brain_kg_edge_list_t* out = brain_kg_get_outgoing(kg, root);
    if (!out) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < out->count; ++i) {
        const brain_kg_edge_t* e = out->edges[i];
        if (!e) continue;
        const brain_kg_node_t* to = brain_kg_get_node(kg, e->to);
        if (!to) continue;
        if (!kind_substr || !*kind_substr) {
            count++;
        } else if ((to->name && strstr(to->name, kind_substr)) ||
                   (to->description && strstr(to->description, kind_substr))) {
            count++;
        }
    }
    brain_kg_edge_list_destroy(out);
    return count;
}
