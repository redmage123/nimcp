//=============================================================================
// nimcp_ml_statistics.c - Machine Learning Statistics Implementation
//=============================================================================
/**
 * @file nimcp_ml_statistics.c
 * @brief Implementation of GMM, GP, HMM, KDE, and Naive Bayes
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include "utils/statistics/nimcp_ml_statistics.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_math_constants.h"

//=============================================================================
// Module Constants
//=============================================================================

#define GMM_MAGIC 0x474D4D01  /* 'GMM\x01' */
#define GP_MAGIC  0x47500001  /* 'GP\x00\x01' */
#define HMM_MAGIC 0x484D4D01  /* 'HMM\x01' */
#define KDE_MAGIC 0x4B444501  /* 'KDE\x01' */
#define NB_MAGIC  0x4E420001  /* 'NB\x00\x01' */

#define LOG_MODULE "ML_STATS"
#define PI_F NIMCP_PI_F

//=============================================================================
// Module State
//=============================================================================

static struct {
    bool initialized;
    nimcp_gpu_context_t* gpu_ctx;
} g_ml_state = {0};

//=============================================================================
// Utility Functions
//=============================================================================

static inline float safe_log(float x) {
    return (x > NIMCP_ML_EPS) ? logf(x) : NIMCP_ML_LOG_EPS;
}

static inline float log_sum_exp(const float* log_vals, uint32_t n) {
    if (n == 0) return NIMCP_ML_LOG_EPS;

    float max_val = log_vals[0];
    for (uint32_t i = 1; i < n; i++) {
        if (log_vals[i] > max_val) max_val = log_vals[i];
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += expf(log_vals[i] - max_val);
    }

    return max_val + logf(sum);
}

static float compute_squared_distance(const float* x1, const float* x2, uint32_t d) {
    float dist = 0.0f;
    for (uint32_t i = 0; i < d; i++) {
        float diff = x1[i] - x2[i];
        dist += diff * diff;
    }
    return dist;
}

static float randn(void) {
    /* Box-Muller transform */
    float u1 = ((float)nimcp_tl_rand() + 1.0f) / ((float)RAND_MAX + 2.0f);
    float u2 = (float)nimcp_tl_rand() / (float)RAND_MAX;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * PI_F * u2);
}

//=============================================================================
// GMM Implementation
//=============================================================================

nimcp_gmm_config_t nimcp_gmm_default_config(void) {
    nimcp_gmm_config_t config = {
        .n_components = 3,
        .cov_type = NIMCP_GMM_COV_DIAGONAL,
        .init_method = NIMCP_GMM_INIT_KMEANS,
        .tol = NIMCP_EM_TOLERANCE,
        .max_iter = NIMCP_EM_MAX_ITER,
        .n_init = 1,
        .reg_covar = 1e-6f,
        .random_seed = 0,
        .use_gpu = false,
        .warm_start = false
    };
    return config;
}

nimcp_gmm_t* nimcp_gmm_create(const nimcp_gmm_config_t* config) {
    nimcp_gmm_config_t cfg = config ? *config : nimcp_gmm_default_config();

    if (cfg.n_components == 0 || cfg.n_components > NIMCP_GMM_MAX_COMPONENTS) {
        LOG_ERROR("Invalid n_components: %u", cfg.n_components);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gmm_create: cfg.n_components is zero");
        return NULL;
    }

    nimcp_gmm_t* gmm = (nimcp_gmm_t*)nimcp_calloc(1, sizeof(nimcp_gmm_t));
    if (!gmm) {
        LOG_ERROR("Failed to allocate GMM");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gmm_create: gmm is NULL");
        return NULL;
    }

    gmm->magic = GMM_MAGIC;
    gmm->n_components = cfg.n_components;
    gmm->cov_type = cfg.cov_type;
    gmm->use_gpu = cfg.use_gpu && g_ml_state.gpu_ctx != NULL;
    gmm->gpu_ctx = g_ml_state.gpu_ctx;
    gmm->is_fitted = false;

    gmm->weights = (float*)nimcp_calloc(cfg.n_components, sizeof(float));
    gmm->log_det = (float*)nimcp_calloc(cfg.n_components, sizeof(float));

    if (!gmm->weights || !gmm->log_det) {
        nimcp_gmm_destroy(gmm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gmm_create: required parameter is NULL (gmm->weights, gmm->log_det)");
        return NULL;
    }

    float init_weight = 1.0f / (float)cfg.n_components;
    for (uint32_t k = 0; k < cfg.n_components; k++) {
        gmm->weights[k] = init_weight;
    }

    return gmm;
}

void nimcp_gmm_destroy(nimcp_gmm_t* gmm) {
    if (!gmm) return;
    if (gmm->magic != GMM_MAGIC) return;

    nimcp_free(gmm->weights);
    nimcp_free(gmm->means);
    nimcp_free(gmm->covariances);
    nimcp_free(gmm->precisions);
    nimcp_free(gmm->precisions_chol);
    nimcp_free(gmm->log_det);

    gmm->magic = 0;
    nimcp_free(gmm);
}

static float gmm_log_gaussian(const float* x, const float* mean,
                              const float* prec, float log_det, uint32_t d) {
    float mahal = 0.0f;
    for (uint32_t j = 0; j < d; j++) {
        float diff = x[j] - mean[j];
        mahal += diff * diff * prec[j];
    }
    return -0.5f * (d * 1.8378770664093453f + log_det + mahal);
}

nimcp_ml_error_t nimcp_gmm_fit(nimcp_gmm_t* gmm, const float* X,
                               uint32_t n_samples, uint32_t n_features,
                               nimcp_gmm_fit_result_t* result) {
    if (!gmm || gmm->magic != GMM_MAGIC) return NIMCP_ML_ERROR_NULL;
    if (!X || n_samples == 0 || n_features == 0) return NIMCP_ML_ERROR_PARAMS;

    uint32_t k = gmm->n_components;
    uint32_t d = n_features;
    uint32_t n = n_samples;

    gmm->n_features = d;

    /* Allocate parameter storage */
    gmm->means = (float*)nimcp_realloc(gmm->means, k * d * sizeof(float));
    gmm->covariances = (float*)nimcp_realloc(gmm->covariances, k * d * sizeof(float));
    gmm->precisions = (float*)nimcp_realloc(gmm->precisions, k * d * sizeof(float));

    if (!gmm->means || !gmm->covariances || !gmm->precisions) {
        return NIMCP_ML_ERROR_MEMORY;
    }

    /* Initialize means with k-means++ */
    /* srand() removed: thread-unsafe global PRNG state; nimcp_tl_rand() has its own seeding */

    /* First centroid: random point */
    uint32_t idx = nimcp_tl_rand() % n;
    memcpy(gmm->means, &X[idx * d], d * sizeof(float));

    /* Remaining centroids: proportional to D^2 */
    float* dists = (float*)nimcp_malloc(n * sizeof(float));
    if (!dists) return NIMCP_ML_ERROR_MEMORY;

    for (uint32_t c = 1; c < k; c++) {
        float total_dist = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            float min_dist = FLT_MAX;
            for (uint32_t j = 0; j < c; j++) {
                float dist = compute_squared_distance(&X[i * d], &gmm->means[j * d], d);
                if (dist < min_dist) min_dist = dist;
            }
            dists[i] = min_dist;
            total_dist += min_dist;
        }

        float target = ((float)nimcp_tl_rand() / (float)RAND_MAX) * total_dist;
        float cum = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            cum += dists[i];
            if (cum >= target) {
                memcpy(&gmm->means[c * d], &X[i * d], d * sizeof(float));
                break;
            }
        }
    }
    nimcp_free(dists);

    /* Initialize covariances */
    for (uint32_t c = 0; c < k; c++) {
        for (uint32_t j = 0; j < d; j++) {
            gmm->covariances[c * d + j] = 1.0f;
            gmm->precisions[c * d + j] = 1.0f;
        }
        gmm->log_det[c] = 0.0f;
    }

    /* Allocate working memory */
    float* resp = (float*)nimcp_malloc(n * k * sizeof(float));
    float* nk = (float*)nimcp_malloc(k * sizeof(float));
    float* log_prob = (float*)nimcp_malloc(k * sizeof(float));

    if (!resp || !nk || !log_prob) {
        nimcp_free(resp); nimcp_free(nk); nimcp_free(log_prob);
        return NIMCP_ML_ERROR_MEMORY;
    }

    float prev_ll = -FLT_MAX;
    uint32_t iter;
    bool converged = false;

    for (iter = 0; iter < NIMCP_EM_MAX_ITER; iter++) {
        /* E-step: compute responsibilities */
        float total_ll = 0.0f;

        for (uint32_t i = 0; i < n; i++) {
            const float* xi = &X[i * d];

            for (uint32_t c = 0; c < k; c++) {
                log_prob[c] = safe_log(gmm->weights[c]) +
                              gmm_log_gaussian(xi, &gmm->means[c * d],
                                             &gmm->precisions[c * d],
                                             gmm->log_det[c], d);
            }

            float log_sum = log_sum_exp(log_prob, k);
            total_ll += log_sum;

            for (uint32_t c = 0; c < k; c++) {
                resp[i * k + c] = expf(log_prob[c] - log_sum);
            }
        }

        if (fabsf(total_ll - prev_ll) < NIMCP_EM_TOLERANCE * fabsf(total_ll)) {
            converged = true;
            gmm->lower_bound = total_ll;
            break;
        }
        prev_ll = total_ll;

        /* M-step */
        memset(nk, 0, k * sizeof(float));
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t c = 0; c < k; c++) {
                nk[c] += resp[i * k + c];
            }
        }

        for (uint32_t c = 0; c < k; c++) {
            gmm->weights[c] = (nk[c] + NIMCP_ML_EPS) / (float)n;
        }

        memset(gmm->means, 0, k * d * sizeof(float));
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t c = 0; c < k; c++) {
                float r = resp[i * k + c];
                for (uint32_t j = 0; j < d; j++) {
                    gmm->means[c * d + j] += r * X[i * d + j];
                }
            }
        }
        for (uint32_t c = 0; c < k; c++) {
            float inv_nk = 1.0f / (nk[c] + NIMCP_ML_EPS);
            for (uint32_t j = 0; j < d; j++) {
                gmm->means[c * d + j] *= inv_nk;
            }
        }

        memset(gmm->covariances, 0, k * d * sizeof(float));
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t c = 0; c < k; c++) {
                float r = resp[i * k + c];
                for (uint32_t j = 0; j < d; j++) {
                    float diff = X[i * d + j] - gmm->means[c * d + j];
                    gmm->covariances[c * d + j] += r * diff * diff;
                }
            }
        }
        for (uint32_t c = 0; c < k; c++) {
            float inv_nk = 1.0f / (nk[c] + NIMCP_ML_EPS);
            gmm->log_det[c] = 0.0f;
            for (uint32_t j = 0; j < d; j++) {
                float var = gmm->covariances[c * d + j] * inv_nk + 1e-6f;
                gmm->covariances[c * d + j] = var;
                gmm->precisions[c * d + j] = 1.0f / var;
                gmm->log_det[c] += logf(var);
            }
        }
    }

    gmm->n_iter = iter;
    gmm->converged = converged;
    gmm->is_fitted = true;
    gmm->lower_bound = prev_ll;

    if (result) {
        result->converged = converged;
        result->n_iter = iter;
        result->log_likelihood = gmm->lower_bound;
        uint32_t n_params = k - 1 + k * d + k * d;
        result->bic = -2.0f * gmm->lower_bound + n_params * logf((float)n);
        result->aic = -2.0f * gmm->lower_bound + 2.0f * n_params;
    }

    nimcp_free(resp);
    nimcp_free(nk);
    nimcp_free(log_prob);

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_gmm_predict(const nimcp_gmm_t* gmm, const float* X,
                                   uint32_t n_samples, uint32_t* labels) {
    if (!gmm || gmm->magic != GMM_MAGIC || !gmm->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!X || !labels) return NIMCP_ML_ERROR_NULL;

    uint32_t k = gmm->n_components;
    uint32_t d = gmm->n_features;

    for (uint32_t i = 0; i < n_samples; i++) {
        const float* xi = &X[i * d];
        float max_log_prob = -FLT_MAX;
        uint32_t best_k = 0;

        for (uint32_t c = 0; c < k; c++) {
            float lp = safe_log(gmm->weights[c]) +
                       gmm_log_gaussian(xi, &gmm->means[c * d],
                                      &gmm->precisions[c * d],
                                      gmm->log_det[c], d);
            if (lp > max_log_prob) {
                max_log_prob = lp;
                best_k = c;
            }
        }
        labels[i] = best_k;
    }

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_gmm_predict_proba(const nimcp_gmm_t* gmm, const float* X,
                                         uint32_t n_samples, float* probs) {
    if (!gmm || gmm->magic != GMM_MAGIC || !gmm->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!X || !probs) return NIMCP_ML_ERROR_NULL;

    uint32_t k = gmm->n_components;
    uint32_t d = gmm->n_features;
    float* log_prob = (float*)nimcp_malloc(k * sizeof(float));
    if (!log_prob) return NIMCP_ML_ERROR_MEMORY;

    for (uint32_t i = 0; i < n_samples; i++) {
        const float* xi = &X[i * d];

        for (uint32_t c = 0; c < k; c++) {
            log_prob[c] = safe_log(gmm->weights[c]) +
                          gmm_log_gaussian(xi, &gmm->means[c * d],
                                         &gmm->precisions[c * d],
                                         gmm->log_det[c], d);
        }

        float log_sum = log_sum_exp(log_prob, k);
        for (uint32_t c = 0; c < k; c++) {
            probs[i * k + c] = expf(log_prob[c] - log_sum);
        }
    }

    nimcp_free(log_prob);
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_gmm_score(const nimcp_gmm_t* gmm, const float* X,
                                 uint32_t n_samples, float* score) {
    if (!gmm || gmm->magic != GMM_MAGIC || !gmm->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!X || !score) return NIMCP_ML_ERROR_NULL;

    uint32_t k = gmm->n_components;
    uint32_t d = gmm->n_features;
    float* log_prob = (float*)nimcp_malloc(k * sizeof(float));
    if (!log_prob) return NIMCP_ML_ERROR_MEMORY;

    float total_ll = 0.0f;
    for (uint32_t i = 0; i < n_samples; i++) {
        const float* xi = &X[i * d];
        for (uint32_t c = 0; c < k; c++) {
            log_prob[c] = safe_log(gmm->weights[c]) +
                          gmm_log_gaussian(xi, &gmm->means[c * d],
                                         &gmm->precisions[c * d],
                                         gmm->log_det[c], d);
        }
        total_ll += log_sum_exp(log_prob, k);
    }

    *score = total_ll / (float)n_samples;
    nimcp_free(log_prob);
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_gmm_score_samples(const nimcp_gmm_t* gmm, const float* X,
                                         uint32_t n_samples, float* scores) {
    if (!gmm || gmm->magic != GMM_MAGIC || !gmm->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!X || !scores) return NIMCP_ML_ERROR_NULL;

    uint32_t k = gmm->n_components;
    uint32_t d = gmm->n_features;
    float* log_prob = (float*)nimcp_malloc(k * sizeof(float));
    if (!log_prob) return NIMCP_ML_ERROR_MEMORY;

    for (uint32_t i = 0; i < n_samples; i++) {
        const float* xi = &X[i * d];
        for (uint32_t c = 0; c < k; c++) {
            log_prob[c] = safe_log(gmm->weights[c]) +
                          gmm_log_gaussian(xi, &gmm->means[c * d],
                                         &gmm->precisions[c * d],
                                         gmm->log_det[c], d);
        }
        scores[i] = log_sum_exp(log_prob, k);
    }

    nimcp_free(log_prob);
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_gmm_bic(const nimcp_gmm_t* gmm, const float* X,
                               uint32_t n_samples, float* bic) {
    float score;
    nimcp_ml_error_t err = nimcp_gmm_score(gmm, X, n_samples, &score);
    if (err != NIMCP_ML_OK) return err;

    uint32_t n_params = gmm->n_components - 1 +
                        gmm->n_components * gmm->n_features +
                        gmm->n_components * gmm->n_features;
    *bic = -2.0f * score * n_samples + n_params * logf((float)n_samples);
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_gmm_aic(const nimcp_gmm_t* gmm, const float* X,
                               uint32_t n_samples, float* aic) {
    float score;
    nimcp_ml_error_t err = nimcp_gmm_score(gmm, X, n_samples, &score);
    if (err != NIMCP_ML_OK) return err;

    uint32_t n_params = gmm->n_components - 1 +
                        gmm->n_components * gmm->n_features +
                        gmm->n_components * gmm->n_features;
    *aic = -2.0f * score * n_samples + 2.0f * n_params;
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_gmm_sample(const nimcp_gmm_t* gmm, uint32_t n_samples,
                                  float* samples, uint32_t* labels) {
    if (!gmm || gmm->magic != GMM_MAGIC || !gmm->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!samples) return NIMCP_ML_ERROR_NULL;

    uint32_t k = gmm->n_components;
    uint32_t d = gmm->n_features;

    for (uint32_t i = 0; i < n_samples; i++) {
        float u = (float)nimcp_tl_rand() / (float)RAND_MAX;
        float cum = 0.0f;
        uint32_t comp = k - 1;
        for (uint32_t c = 0; c < k; c++) {
            cum += gmm->weights[c];
            if (u <= cum) { comp = c; break; }
        }

        if (labels) labels[i] = comp;

        for (uint32_t j = 0; j < d; j++) {
            float z = randn();
            samples[i * d + j] = gmm->means[comp * d + j] +
                                 sqrtf(gmm->covariances[comp * d + j]) * z;
        }
    }

    return NIMCP_ML_OK;
}

//=============================================================================
// Gaussian Process Implementation
//=============================================================================

nimcp_gp_config_t nimcp_gp_default_config(void) {
    nimcp_gp_config_t config = {
        .kernel = {
            .type = NIMCP_GP_KERNEL_RBF,
            .length_scale = 1.0f,
            .variance = 1.0f,
            .period = 1.0f,
            .alpha = 1.0f,
            .degree = 3,
            .length_scales = NULL,
            .use_ard = false
        },
        .noise_variance = 0.1f,
        .normalize_y = true,
        .optimize_hyperparams = false,
        .optimizer_max_iter = 100,
        .optimizer_tol = 1e-5f,
        .random_seed = 0,
        .use_gpu = false,
        .jitter = 1e-6f
    };
    return config;
}

nimcp_gp_kernel_params_t nimcp_gp_kernel_default(nimcp_gp_kernel_t type) {
    nimcp_gp_kernel_params_t params = {
        .type = type,
        .length_scale = 1.0f,
        .variance = 1.0f,
        .period = 1.0f,
        .alpha = 1.0f,
        .degree = 3,
        .length_scales = NULL,
        .use_ard = false
    };
    return params;
}

nimcp_gp_t* nimcp_gp_create(const nimcp_gp_config_t* config) {
    nimcp_gp_config_t cfg = config ? *config : nimcp_gp_default_config();

    nimcp_gp_t* gp = (nimcp_gp_t*)nimcp_calloc(1, sizeof(nimcp_gp_t));
    if (!gp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gp_create: gp is NULL");
        return NULL;
    }

    gp->magic = GP_MAGIC;
    gp->kernel = cfg.kernel;
    gp->noise_variance = cfg.noise_variance;
    gp->normalize_y = cfg.normalize_y;
    gp->use_gpu = cfg.use_gpu && g_ml_state.gpu_ctx != NULL;
    gp->gpu_ctx = g_ml_state.gpu_ctx;
    gp->is_fitted = false;

    return gp;
}

void nimcp_gp_destroy(nimcp_gp_t* gp) {
    if (!gp) return;
    if (gp->magic != GP_MAGIC) return;

    nimcp_free(gp->X_train);
    nimcp_free(gp->y_train);
    nimcp_free(gp->L);
    nimcp_free(gp->alpha);
    nimcp_free(gp->kernel.length_scales);

    gp->magic = 0;
    nimcp_free(gp);
}

static float gp_kernel_rbf(const float* x1, const float* x2, uint32_t d,
                           float length_scale, float variance) {
    float sq_dist = 0.0f;
    float inv_l2 = 1.0f / (length_scale * length_scale);
    for (uint32_t i = 0; i < d; i++) {
        float diff = x1[i] - x2[i];
        sq_dist += diff * diff;
    }
    return variance * expf(-0.5f * sq_dist * inv_l2);
}

static float gp_kernel_matern32(const float* x1, const float* x2, uint32_t d,
                                float length_scale, float variance) {
    float dist = 0.0f;
    for (uint32_t i = 0; i < d; i++) {
        float diff = x1[i] - x2[i];
        dist += diff * diff;
    }
    dist = sqrtf(dist) / length_scale;
    float sqrt3_r = sqrtf(3.0f) * dist;
    return variance * (1.0f + sqrt3_r) * expf(-sqrt3_r);
}

static float gp_kernel_matern52(const float* x1, const float* x2, uint32_t d,
                                float length_scale, float variance) {
    float dist = 0.0f;
    for (uint32_t i = 0; i < d; i++) {
        float diff = x1[i] - x2[i];
        dist += diff * diff;
    }
    dist = sqrtf(dist) / length_scale;
    float sqrt5_r = sqrtf(5.0f) * dist;
    return variance * (1.0f + sqrt5_r + sqrt5_r * sqrt5_r / 3.0f) * expf(-sqrt5_r);
}

static float gp_compute_kernel(const nimcp_gp_kernel_params_t* k,
                               const float* x1, const float* x2, uint32_t d) {
    switch (k->type) {
        case NIMCP_GP_KERNEL_RBF:
            return gp_kernel_rbf(x1, x2, d, k->length_scale, k->variance);
        case NIMCP_GP_KERNEL_MATERN_32:
            return gp_kernel_matern32(x1, x2, d, k->length_scale, k->variance);
        case NIMCP_GP_KERNEL_MATERN_52:
            return gp_kernel_matern52(x1, x2, d, k->length_scale, k->variance);
        case NIMCP_GP_KERNEL_MATERN_12: {
            float dist = sqrtf(compute_squared_distance(x1, x2, d)) / k->length_scale;
            return k->variance * expf(-dist);
        }
        case NIMCP_GP_KERNEL_LINEAR: {
            float dot = 0.0f;
            for (uint32_t i = 0; i < d; i++) dot += x1[i] * x2[i];
            return k->variance * dot;
        }
        default:
            return gp_kernel_rbf(x1, x2, d, k->length_scale, k->variance);
    }
}

/* Simple Cholesky decomposition */
static bool cholesky(float* A, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            float sum = A[i * n + j];
            for (uint32_t k = 0; k < j; k++) {
                sum -= A[i * n + k] * A[j * n + k];
            }
            if (i == j) {
                if (sum <= 0.0f) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cholesky: validation failed");
                    return false;
                }
                A[i * n + j] = sqrtf(sum);
            } else {
                A[i * n + j] = sum / A[j * n + j];
            }
        }
        for (uint32_t j = i + 1; j < n; j++) {
            A[i * n + j] = 0.0f;
        }
    }
    return true;
}

/* Solve L*x = b where L is lower triangular */
static void solve_lower(const float* L, const float* b, float* x, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        float sum = b[i];
        for (uint32_t j = 0; j < i; j++) {
            sum -= L[i * n + j] * x[j];
        }
        x[i] = sum / L[i * n + i];
    }
}

/* Solve L^T * x = b where L is lower triangular */
static void solve_upper(const float* L, const float* b, float* x, uint32_t n) {
    for (int i = (int)n - 1; i >= 0; i--) {
        float sum = b[i];
        for (uint32_t j = i + 1; j < n; j++) {
            sum -= L[j * n + i] * x[j];
        }
        x[i] = sum / L[i * n + i];
    }
}

nimcp_ml_error_t nimcp_gp_fit(nimcp_gp_t* gp, const float* X, const float* y,
                              uint32_t n_samples, uint32_t n_features) {
    if (!gp || gp->magic != GP_MAGIC) return NIMCP_ML_ERROR_NULL;
    if (!X || !y || n_samples == 0) return NIMCP_ML_ERROR_PARAMS;

    uint32_t n = n_samples;
    uint32_t d = n_features;

    gp->n_train = n;
    gp->n_features = d;

    /* Integer overflow check for quadratic allocations */
    if (n > 0 && ((size_t)n * n > SIZE_MAX / sizeof(float) ||
                   (size_t)n * d > SIZE_MAX / sizeof(float))) {
        return NIMCP_ML_ERROR_MEMORY;
    }

    /* Store training data */
    gp->X_train = (float*)nimcp_realloc(gp->X_train, (size_t)n * d * sizeof(float));
    gp->y_train = (float*)nimcp_realloc(gp->y_train, n * sizeof(float));
    gp->L = (float*)nimcp_realloc(gp->L, (size_t)n * n * sizeof(float));
    gp->alpha = (float*)nimcp_realloc(gp->alpha, n * sizeof(float));

    if (!gp->X_train || !gp->y_train || !gp->L || !gp->alpha) {
        return NIMCP_ML_ERROR_MEMORY;
    }

    memcpy(gp->X_train, X, n * d * sizeof(float));

    /* Normalize y if requested */
    gp->y_mean = 0.0f;
    gp->y_std = 1.0f;
    if (gp->normalize_y) {
        for (uint32_t i = 0; i < n; i++) gp->y_mean += y[i];
        gp->y_mean /= (float)n;

        for (uint32_t i = 0; i < n; i++) {
            float diff = y[i] - gp->y_mean;
            gp->y_std += diff * diff;
        }
        gp->y_std = sqrtf(gp->y_std / (float)n);
        if (gp->y_std < NIMCP_ML_EPS) gp->y_std = 1.0f;

        for (uint32_t i = 0; i < n; i++) {
            gp->y_train[i] = (y[i] - gp->y_mean) / gp->y_std;
        }
    } else {
        memcpy(gp->y_train, y, n * sizeof(float));
    }

    /* Compute kernel matrix K + noise*I */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            float k_val = gp_compute_kernel(&gp->kernel,
                                           &gp->X_train[i * d],
                                           &gp->X_train[j * d], d);
            if (i == j) k_val += gp->noise_variance + 1e-6f;
            gp->L[i * n + j] = k_val;
            gp->L[j * n + i] = k_val;
        }
    }

    /* Cholesky decomposition */
    if (!cholesky(gp->L, n)) {
        return NIMCP_ML_ERROR_SINGULAR;
    }

    /* Compute alpha = L^T \ (L \ y) */
    float* tmp = (float*)nimcp_malloc(n * sizeof(float));
    if (!tmp) return NIMCP_ML_ERROR_MEMORY;

    solve_lower(gp->L, gp->y_train, tmp, n);
    solve_upper(gp->L, tmp, gp->alpha, n);

    /* Compute log marginal likelihood */
    float data_fit = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        data_fit += gp->y_train[i] * gp->alpha[i];
    }

    float log_det = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        log_det += logf(gp->L[i * n + i]);
    }
    log_det *= 2.0f;

    gp->log_marginal_likelihood = -0.5f * data_fit - 0.5f * log_det -
                                  0.5f * n * logf(2.0f * PI_F);

    nimcp_free(tmp);
    gp->is_fitted = true;

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_gp_predict(const nimcp_gp_t* gp, const float* X_test,
                                  uint32_t n_test, float* y_pred) {
    if (!gp || gp->magic != GP_MAGIC || !gp->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!X_test || !y_pred) return NIMCP_ML_ERROR_NULL;

    uint32_t n = gp->n_train;
    uint32_t d = gp->n_features;

    for (uint32_t i = 0; i < n_test; i++) {
        float mean = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            float k_star = gp_compute_kernel(&gp->kernel,
                                            &X_test[i * d],
                                            &gp->X_train[j * d], d);
            mean += k_star * gp->alpha[j];
        }

        if (gp->normalize_y) {
            y_pred[i] = mean * gp->y_std + gp->y_mean;
        } else {
            y_pred[i] = mean;
        }
    }

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_gp_predict_with_std(const nimcp_gp_t* gp, const float* X_test,
                                           uint32_t n_test, float* y_pred, float* y_std) {
    if (!gp || gp->magic != GP_MAGIC || !gp->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!X_test || !y_pred || !y_std) return NIMCP_ML_ERROR_NULL;

    uint32_t n = gp->n_train;
    uint32_t d = gp->n_features;

    float* k_star = (float*)nimcp_malloc(n * sizeof(float));
    float* v = (float*)nimcp_malloc(n * sizeof(float));
    if (!k_star || !v) {
        nimcp_free(k_star); nimcp_free(v);
        return NIMCP_ML_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < n_test; i++) {
        /* Compute k_star */
        for (uint32_t j = 0; j < n; j++) {
            k_star[j] = gp_compute_kernel(&gp->kernel,
                                         &X_test[i * d],
                                         &gp->X_train[j * d], d);
        }

        /* Mean: k_star^T * alpha */
        float mean = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            mean += k_star[j] * gp->alpha[j];
        }

        /* Variance: k(x*, x*) - v^T * v where v = L \ k_star */
        solve_lower(gp->L, k_star, v, n);

        float k_ss = gp_compute_kernel(&gp->kernel, &X_test[i * d], &X_test[i * d], d);
        float var = k_ss;
        for (uint32_t j = 0; j < n; j++) {
            var -= v[j] * v[j];
        }
        if (var < 0.0f) var = 0.0f;

        if (gp->normalize_y) {
            y_pred[i] = mean * gp->y_std + gp->y_mean;
            y_std[i] = sqrtf(var) * gp->y_std;
        } else {
            y_pred[i] = mean;
            y_std[i] = sqrtf(var);
        }
    }

    nimcp_free(k_star);
    nimcp_free(v);
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_gp_log_marginal_likelihood(const nimcp_gp_t* gp, float* log_ml) {
    if (!gp || gp->magic != GP_MAGIC || !gp->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!log_ml) return NIMCP_ML_ERROR_NULL;

    *log_ml = gp->log_marginal_likelihood;
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_gp_kernel_matrix(const nimcp_gp_kernel_params_t* kernel,
                                        const float* X1, const float* X2,
                                        uint32_t n1, uint32_t n2, uint32_t n_features,
                                        float* K) {
    if (!kernel || !X1 || !X2 || !K) return NIMCP_ML_ERROR_NULL;

    for (uint32_t i = 0; i < n1; i++) {
        for (uint32_t j = 0; j < n2; j++) {
            K[i * n2 + j] = gp_compute_kernel(kernel,
                                             &X1[i * n_features],
                                             &X2[j * n_features],
                                             n_features);
        }
    }

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_gp_sample(const nimcp_gp_t* gp, const float* X_test,
                                 uint32_t n_test, uint32_t n_samples, float* samples) {
    if (!gp || gp->magic != GP_MAGIC || !gp->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!X_test || !samples) return NIMCP_ML_ERROR_NULL;

    /* Get mean and std */
    float* mean = (float*)nimcp_malloc(n_test * sizeof(float));
    float* std = (float*)nimcp_malloc(n_test * sizeof(float));
    if (!mean || !std) {
        nimcp_free(mean); nimcp_free(std);
        return NIMCP_ML_ERROR_MEMORY;
    }

    nimcp_ml_error_t err = nimcp_gp_predict_with_std(gp, X_test, n_test, mean, std);
    if (err != NIMCP_ML_OK) {
        nimcp_free(mean); nimcp_free(std);
        return err;
    }

    /* Sample: mean + std * N(0,1) */
    for (uint32_t s = 0; s < n_samples; s++) {
        for (uint32_t i = 0; i < n_test; i++) {
            samples[s * n_test + i] = mean[i] + std[i] * randn();
        }
    }

    nimcp_free(mean);
    nimcp_free(std);
    return NIMCP_ML_OK;
}

//=============================================================================
// HMM Implementation (continued in next part)
//=============================================================================

nimcp_hmm_config_t nimcp_hmm_default_config(void) {
    nimcp_hmm_config_t config = {
        .n_states = 3,
        .emission_type = NIMCP_HMM_EMISSION_GAUSSIAN,
        .n_features = 1,
        .n_gmm_components = 1,
        .tol = 1e-4f,
        .max_iter = 100,
        .left_right = false,
        .random_seed = 0,
        .use_gpu = false
    };
    return config;
}

nimcp_hmm_t* nimcp_hmm_create(const nimcp_hmm_config_t* config) {
    nimcp_hmm_config_t cfg = config ? *config : nimcp_hmm_default_config();

    if (cfg.n_states == 0 || cfg.n_states > NIMCP_HMM_MAX_STATES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hmm_create: cfg.n_states is zero");
        return NULL;
    }

    nimcp_hmm_t* hmm = (nimcp_hmm_t*)nimcp_calloc(1, sizeof(nimcp_hmm_t));
    if (!hmm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hmm_create: hmm is NULL");
        return NULL;
    }

    hmm->magic = HMM_MAGIC;
    hmm->n_states = cfg.n_states;
    hmm->n_features = cfg.n_features;
    hmm->emission_type = cfg.emission_type;
    hmm->use_gpu = cfg.use_gpu && g_ml_state.gpu_ctx != NULL;
    hmm->gpu_ctx = g_ml_state.gpu_ctx;

    uint32_t s = cfg.n_states;
    uint32_t d = cfg.n_features;

    hmm->initial_prob = (float*)nimcp_calloc(s, sizeof(float));
    hmm->transition_prob = (float*)nimcp_calloc(s * s, sizeof(float));
    hmm->log_initial = (float*)nimcp_calloc(s, sizeof(float));
    hmm->log_transition = (float*)nimcp_calloc(s * s, sizeof(float));

    if (!hmm->initial_prob || !hmm->transition_prob ||
        !hmm->log_initial || !hmm->log_transition) {
        nimcp_hmm_destroy(hmm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_hmm_create: operation failed");
        return NULL;
    }

    /* Initialize uniform */
    float init_p = 1.0f / (float)s;
    for (uint32_t i = 0; i < s; i++) {
        hmm->initial_prob[i] = init_p;
        hmm->log_initial[i] = safe_log(init_p);
        for (uint32_t j = 0; j < s; j++) {
            hmm->transition_prob[i * s + j] = init_p;
            hmm->log_transition[i * s + j] = safe_log(init_p);
        }
    }

    if (cfg.emission_type == NIMCP_HMM_EMISSION_GAUSSIAN) {
        hmm->emission_means = (float*)nimcp_calloc(s * d, sizeof(float));
        hmm->emission_covars = (float*)nimcp_calloc(s * d, sizeof(float));
        if (!hmm->emission_means || !hmm->emission_covars) {
            nimcp_hmm_destroy(hmm);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hmm_create: required parameter is NULL (hmm->emission_means, hmm->emission_covars)");
            return NULL;
        }
        for (uint32_t i = 0; i < s * d; i++) {
            hmm->emission_covars[i] = 1.0f;
        }
    }

    return hmm;
}

void nimcp_hmm_destroy(nimcp_hmm_t* hmm) {
    if (!hmm) return;
    if (hmm->magic != HMM_MAGIC) return;

    nimcp_free(hmm->initial_prob);
    nimcp_free(hmm->transition_prob);
    nimcp_free(hmm->log_initial);
    nimcp_free(hmm->log_transition);
    nimcp_free(hmm->emission_means);
    nimcp_free(hmm->emission_covars);
    nimcp_free(hmm->emission_probs);
    nimcp_free(hmm->emission_rates);

    if (hmm->state_gmms) {
        for (uint32_t i = 0; i < hmm->n_states; i++) {
            nimcp_gmm_destroy(hmm->state_gmms[i]);
        }
        nimcp_free(hmm->state_gmms);
    }

    hmm->magic = 0;
    nimcp_free(hmm);
}

static float hmm_log_emission(const nimcp_hmm_t* hmm, uint32_t state,
                              const float* obs) {
    uint32_t d = hmm->n_features;
    const float* mean = &hmm->emission_means[state * d];
    const float* var = &hmm->emission_covars[state * d];

    float log_prob = 0.0f;
    for (uint32_t j = 0; j < d; j++) {
        float diff = obs[j] - mean[j];
        log_prob += -0.5f * (logf(2.0f * PI_F * var[j]) + diff * diff / var[j]);
    }
    return log_prob;
}

nimcp_ml_error_t nimcp_hmm_fit(nimcp_hmm_t* hmm, const float* observations,
                               const uint32_t* seq_lengths, uint32_t n_sequences) {
    if (!hmm || hmm->magic != HMM_MAGIC) return NIMCP_ML_ERROR_NULL;
    if (!observations || !seq_lengths || n_sequences == 0) return NIMCP_ML_ERROR_PARAMS;

    uint32_t s = hmm->n_states;
    uint32_t d = hmm->n_features;

    /* Get total length */
    uint32_t total_len = 0;
    for (uint32_t seq = 0; seq < n_sequences; seq++) {
        total_len += seq_lengths[seq];
    }

    /* Initialize emission means from data */
    /* srand() removed: thread-unsafe global PRNG state; nimcp_tl_rand() has its own seeding */
    for (uint32_t i = 0; i < s; i++) {
        uint32_t idx = nimcp_tl_rand() % total_len;
        memcpy(&hmm->emission_means[i * d], &observations[idx * d], d * sizeof(float));
    }

    /* Compute data variance for initialization */
    float* data_mean = (float*)nimcp_calloc(d, sizeof(float));
    float* data_var = (float*)nimcp_calloc(d, sizeof(float));
    if (!data_mean || !data_var) {
        nimcp_free(data_mean); nimcp_free(data_var);
        return NIMCP_ML_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < total_len; i++) {
        for (uint32_t j = 0; j < d; j++) {
            data_mean[j] += observations[i * d + j];
        }
    }
    for (uint32_t j = 0; j < d; j++) {
        data_mean[j] /= (float)total_len;
    }

    for (uint32_t i = 0; i < total_len; i++) {
        for (uint32_t j = 0; j < d; j++) {
            float diff = observations[i * d + j] - data_mean[j];
            data_var[j] += diff * diff;
        }
    }
    for (uint32_t j = 0; j < d; j++) {
        data_var[j] /= (float)total_len;
        if (data_var[j] < NIMCP_ML_EPS) data_var[j] = 1.0f;
    }

    for (uint32_t i = 0; i < s; i++) {
        for (uint32_t j = 0; j < d; j++) {
            hmm->emission_covars[i * d + j] = data_var[j];
        }
    }

    nimcp_free(data_mean);
    nimcp_free(data_var);

    /* Allocate working memory */
    uint32_t max_len = 0;
    for (uint32_t seq = 0; seq < n_sequences; seq++) {
        if (seq_lengths[seq] > max_len) max_len = seq_lengths[seq];
    }

    float* alpha = (float*)nimcp_malloc(max_len * s * sizeof(float));
    float* beta = (float*)nimcp_malloc(max_len * s * sizeof(float));
    float* gamma = (float*)nimcp_malloc(max_len * s * sizeof(float));
    float* xi_sum = (float*)nimcp_calloc(s * s, sizeof(float));
    float* gamma_sum = (float*)nimcp_calloc(s, sizeof(float));
    float* gamma_init_sum = (float*)nimcp_calloc(s, sizeof(float));
    float* scale = (float*)nimcp_malloc(max_len * sizeof(float));

    /* Pre-allocate loop scratch buffers on heap instead of alloca() to avoid
     * stack overflow with large state counts (s*s is quadratic). */
    float* log_tmp_buf = (float*)nimcp_malloc(s * sizeof(float));
    float* log_gamma_buf = (float*)nimcp_malloc(s * sizeof(float));
    float* log_xi_buf = (float*)nimcp_malloc(s * s * sizeof(float));

    if (!alpha || !beta || !gamma || !xi_sum || !gamma_sum || !gamma_init_sum || !scale ||
        !log_tmp_buf || !log_gamma_buf || !log_xi_buf) {
        nimcp_free(alpha); nimcp_free(beta); nimcp_free(gamma);
        nimcp_free(xi_sum); nimcp_free(gamma_sum); nimcp_free(gamma_init_sum); nimcp_free(scale);
        nimcp_free(log_tmp_buf); nimcp_free(log_gamma_buf); nimcp_free(log_xi_buf);
        return NIMCP_ML_ERROR_MEMORY;
    }

    float prev_ll = -FLT_MAX;
    uint32_t iter;
    bool converged = false;

    for (iter = 0; iter < 100; iter++) {
        float total_ll = 0.0f;

        memset(xi_sum, 0, s * s * sizeof(float));
        memset(gamma_sum, 0, s * sizeof(float));
        memset(gamma_init_sum, 0, s * sizeof(float));

        float* mean_num = (float*)nimcp_calloc(s * d, sizeof(float));
        float* var_num = (float*)nimcp_calloc(s * d, sizeof(float));

        uint32_t obs_offset = 0;
        for (uint32_t seq = 0; seq < n_sequences; seq++) {
            uint32_t T = seq_lengths[seq];
            const float* obs = &observations[obs_offset * d];

            /* Forward pass */
            for (uint32_t i = 0; i < s; i++) {
                alpha[i] = hmm->log_initial[i] + hmm_log_emission(hmm, i, &obs[0]);
            }

            float log_scale = log_sum_exp(alpha, s);
            scale[0] = log_scale;
            for (uint32_t i = 0; i < s; i++) {
                alpha[i] -= log_scale;
            }

            for (uint32_t t = 1; t < T; t++) {
                for (uint32_t j = 0; j < s; j++) {
                    for (uint32_t i = 0; i < s; i++) {
                        log_tmp_buf[i] = alpha[(t-1) * s + i] + hmm->log_transition[i * s + j];
                    }
                    alpha[t * s + j] = log_sum_exp(log_tmp_buf, s) +
                                       hmm_log_emission(hmm, j, &obs[t * d]);
                }

                log_scale = log_sum_exp(&alpha[t * s], s);
                scale[t] = log_scale;
                for (uint32_t i = 0; i < s; i++) {
                    alpha[t * s + i] -= log_scale;
                }
            }

            /* Accumulate log-likelihood */
            for (uint32_t t = 0; t < T; t++) {
                total_ll += scale[t];
            }

            /* Backward pass */
            for (uint32_t i = 0; i < s; i++) {
                beta[(T-1) * s + i] = 0.0f;
            }

            for (int t = (int)T - 2; t >= 0; t--) {
                for (uint32_t i = 0; i < s; i++) {
                    for (uint32_t j = 0; j < s; j++) {
                        log_tmp_buf[j] = hmm->log_transition[i * s + j] +
                                         hmm_log_emission(hmm, j, &obs[(t+1) * d]) +
                                         beta[(t+1) * s + j];
                    }
                    beta[t * s + i] = log_sum_exp(log_tmp_buf, s) - scale[t+1];
                }
            }

            /* Compute gamma and accumulate */
            for (uint32_t t = 0; t < T; t++) {
                for (uint32_t i = 0; i < s; i++) {
                    log_gamma_buf[i] = alpha[t * s + i] + beta[t * s + i];
                }
                float log_sum = log_sum_exp(log_gamma_buf, s);

                for (uint32_t i = 0; i < s; i++) {
                    gamma[t * s + i] = expf(log_gamma_buf[i] - log_sum);
                    gamma_sum[i] += gamma[t * s + i];

                    for (uint32_t j = 0; j < d; j++) {
                        mean_num[i * d + j] += gamma[t * s + i] * obs[t * d + j];
                    }
                }

                if (t == 0) {
                    for (uint32_t i = 0; i < s; i++) {
                        gamma_init_sum[i] += gamma[i];
                    }
                }
            }

            /* Accumulate xi */
            for (uint32_t t = 0; t < T - 1; t++) {
                for (uint32_t i = 0; i < s; i++) {
                    for (uint32_t j = 0; j < s; j++) {
                        log_xi_buf[i * s + j] = alpha[t * s + i] +
                                           hmm->log_transition[i * s + j] +
                                           hmm_log_emission(hmm, j, &obs[(t+1) * d]) +
                                           beta[(t+1) * s + j];
                    }
                }
                float log_sum = log_sum_exp(log_xi_buf, s * s);

                for (uint32_t i = 0; i < s; i++) {
                    for (uint32_t j = 0; j < s; j++) {
                        xi_sum[i * s + j] += expf(log_xi_buf[i * s + j] - log_sum);
                    }
                }
            }

            obs_offset += T;
        }

        /* Check convergence */
        if (fabsf(total_ll - prev_ll) < 1e-4f * fabsf(total_ll)) {
            converged = true;
            hmm->log_likelihood = total_ll;
            nimcp_free(mean_num); nimcp_free(var_num);
            break;
        }
        prev_ll = total_ll;

        /* M-step: update parameters */

        /* Initial probabilities */
        float init_sum = 0.0f;
        for (uint32_t i = 0; i < s; i++) init_sum += gamma_init_sum[i];
        for (uint32_t i = 0; i < s; i++) {
            hmm->initial_prob[i] = (gamma_init_sum[i] + NIMCP_ML_EPS) / (init_sum + s * NIMCP_ML_EPS);
            hmm->log_initial[i] = safe_log(hmm->initial_prob[i]);
        }

        /* Transition probabilities */
        for (uint32_t i = 0; i < s; i++) {
            float row_sum = 0.0f;
            for (uint32_t j = 0; j < s; j++) row_sum += xi_sum[i * s + j];
            for (uint32_t j = 0; j < s; j++) {
                hmm->transition_prob[i * s + j] = (xi_sum[i * s + j] + NIMCP_ML_EPS) /
                                                  (row_sum + s * NIMCP_ML_EPS);
                hmm->log_transition[i * s + j] = safe_log(hmm->transition_prob[i * s + j]);
            }
        }

        /* Emission means */
        for (uint32_t i = 0; i < s; i++) {
            for (uint32_t j = 0; j < d; j++) {
                hmm->emission_means[i * d + j] = mean_num[i * d + j] /
                                                 (gamma_sum[i] + NIMCP_ML_EPS);
            }
        }

        /* Emission variances */
        obs_offset = 0;
        for (uint32_t seq = 0; seq < n_sequences; seq++) {
            uint32_t T = seq_lengths[seq];
            const float* obs = &observations[obs_offset * d];

            for (uint32_t t = 0; t < T; t++) {
                for (uint32_t i = 0; i < s; i++) {
                    for (uint32_t j = 0; j < d; j++) {
                        float diff = obs[t * d + j] - hmm->emission_means[i * d + j];
                        var_num[i * d + j] += gamma[t * s + i] * diff * diff;
                    }
                }
            }
            obs_offset += T;
        }

        for (uint32_t i = 0; i < s; i++) {
            for (uint32_t j = 0; j < d; j++) {
                hmm->emission_covars[i * d + j] = var_num[i * d + j] /
                                                  (gamma_sum[i] + NIMCP_ML_EPS);
                if (hmm->emission_covars[i * d + j] < 1e-4f) {
                    hmm->emission_covars[i * d + j] = 1e-4f;
                }
            }
        }

        nimcp_free(mean_num);
        nimcp_free(var_num);
    }

    hmm->n_iter = iter;
    hmm->converged = converged;
    hmm->is_fitted = true;

    nimcp_free(alpha); nimcp_free(beta); nimcp_free(gamma);
    nimcp_free(xi_sum); nimcp_free(gamma_sum); nimcp_free(gamma_init_sum); nimcp_free(scale);
    nimcp_free(log_tmp_buf); nimcp_free(log_gamma_buf); nimcp_free(log_xi_buf);

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_hmm_decode(const nimcp_hmm_t* hmm, const float* observations,
                                  uint32_t length, nimcp_hmm_decode_result_t* result) {
    if (!hmm || hmm->magic != HMM_MAGIC || !hmm->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!observations || !result || length == 0) return NIMCP_ML_ERROR_NULL;

    uint32_t s = hmm->n_states;
    uint32_t d = hmm->n_features;
    uint32_t T = length;

    float* viterbi = (float*)nimcp_malloc(T * s * sizeof(float));
    uint32_t* backptr = (uint32_t*)nimcp_malloc(T * s * sizeof(uint32_t));
    result->states = (uint32_t*)nimcp_malloc(T * sizeof(uint32_t));

    if (!viterbi || !backptr || !result->states) {
        nimcp_free(viterbi); nimcp_free(backptr); nimcp_free(result->states);
        result->states = NULL;
        return NIMCP_ML_ERROR_MEMORY;
    }

    /* Initialize */
    for (uint32_t i = 0; i < s; i++) {
        viterbi[i] = hmm->log_initial[i] + hmm_log_emission(hmm, i, &observations[0]);
        backptr[i] = 0;
    }

    /* Forward pass */
    for (uint32_t t = 1; t < T; t++) {
        for (uint32_t j = 0; j < s; j++) {
            float max_val = -FLT_MAX;
            uint32_t max_idx = 0;

            for (uint32_t i = 0; i < s; i++) {
                float val = viterbi[(t-1) * s + i] + hmm->log_transition[i * s + j];
                if (val > max_val) {
                    max_val = val;
                    max_idx = i;
                }
            }

            viterbi[t * s + j] = max_val + hmm_log_emission(hmm, j, &observations[t * d]);
            backptr[t * s + j] = max_idx;
        }
    }

    /* Find best final state */
    float max_val = -FLT_MAX;
    uint32_t max_idx = 0;
    for (uint32_t i = 0; i < s; i++) {
        if (viterbi[(T-1) * s + i] > max_val) {
            max_val = viterbi[(T-1) * s + i];
            max_idx = i;
        }
    }

    result->log_likelihood = max_val;
    result->length = T;

    /* Backtrack */
    result->states[T-1] = max_idx;
    for (int t = (int)T - 2; t >= 0; t--) {
        result->states[t] = backptr[(t+1) * s + result->states[t+1]];
    }

    nimcp_free(viterbi);
    nimcp_free(backptr);

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_hmm_predict(const nimcp_hmm_t* hmm, const float* observations,
                                   uint32_t length, float* state_probs, float* log_likelihood) {
    if (!hmm || hmm->magic != HMM_MAGIC || !hmm->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!observations || !state_probs) return NIMCP_ML_ERROR_NULL;

    uint32_t s = hmm->n_states;
    uint32_t d = hmm->n_features;
    uint32_t T = length;

    float* alpha = (float*)nimcp_malloc(T * s * sizeof(float));
    float* scale = (float*)nimcp_malloc(T * sizeof(float));
    float* log_alpha_buf = (float*)nimcp_malloc(s * sizeof(float));
    float* log_tmp_pred = (float*)nimcp_malloc(s * sizeof(float));
    if (!alpha || !scale || !log_alpha_buf || !log_tmp_pred) {
        nimcp_free(alpha); nimcp_free(scale);
        nimcp_free(log_alpha_buf); nimcp_free(log_tmp_pred);
        return NIMCP_ML_ERROR_MEMORY;
    }

    /* Forward pass */
    for (uint32_t i = 0; i < s; i++) {
        alpha[i] = hmm->log_initial[i] + hmm_log_emission(hmm, i, &observations[0]);
    }

    float log_scale = log_sum_exp(alpha, s);
    scale[0] = log_scale;
    for (uint32_t i = 0; i < s; i++) {
        alpha[i] = expf(alpha[i] - log_scale);
        state_probs[i] = alpha[i];
    }

    for (uint32_t t = 1; t < T; t++) {
        for (uint32_t j = 0; j < s; j++) {
            for (uint32_t i = 0; i < s; i++) {
                log_tmp_pred[i] = safe_log(alpha[(t-1) * s + i]) + hmm->log_transition[i * s + j];
            }
            log_alpha_buf[j] = log_sum_exp(log_tmp_pred, s) + hmm_log_emission(hmm, j, &observations[t * d]);
        }

        log_scale = log_sum_exp(log_alpha_buf, s);
        scale[t] = log_scale;

        for (uint32_t i = 0; i < s; i++) {
            alpha[t * s + i] = expf(log_alpha_buf[i] - log_scale);
            state_probs[t * s + i] = alpha[t * s + i];
        }
    }

    if (log_likelihood) {
        *log_likelihood = 0.0f;
        for (uint32_t t = 0; t < T; t++) {
            *log_likelihood += scale[t];
        }
    }

    nimcp_free(alpha);
    nimcp_free(scale);
    nimcp_free(log_alpha_buf);
    nimcp_free(log_tmp_pred);
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_hmm_sample(const nimcp_hmm_t* hmm, uint32_t length,
                                  float* observations, uint32_t* states) {
    if (!hmm || hmm->magic != HMM_MAGIC || !hmm->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!observations || !states) return NIMCP_ML_ERROR_NULL;

    uint32_t s = hmm->n_states;
    uint32_t d = hmm->n_features;

    /* Sample initial state */
    float u = (float)nimcp_tl_rand() / (float)RAND_MAX;
    float cum = 0.0f;
    states[0] = s - 1;
    for (uint32_t i = 0; i < s; i++) {
        cum += hmm->initial_prob[i];
        if (u <= cum) { states[0] = i; break; }
    }

    /* Sample observation */
    for (uint32_t j = 0; j < d; j++) {
        float z = randn();
        observations[j] = hmm->emission_means[states[0] * d + j] +
                         sqrtf(hmm->emission_covars[states[0] * d + j]) * z;
    }

    /* Sample sequence */
    for (uint32_t t = 1; t < length; t++) {
        uint32_t prev = states[t-1];

        u = (float)nimcp_tl_rand() / (float)RAND_MAX;
        cum = 0.0f;
        states[t] = s - 1;
        for (uint32_t i = 0; i < s; i++) {
            cum += hmm->transition_prob[prev * s + i];
            if (u <= cum) { states[t] = i; break; }
        }

        for (uint32_t j = 0; j < d; j++) {
            float z = randn();
            observations[t * d + j] = hmm->emission_means[states[t] * d + j] +
                                      sqrtf(hmm->emission_covars[states[t] * d + j]) * z;
        }
    }

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_hmm_score(const nimcp_hmm_t* hmm, const float* observations,
                                 uint32_t length, float* log_likelihood) {
    if (!hmm || hmm->magic != HMM_MAGIC || !hmm->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!observations || !log_likelihood) return NIMCP_ML_ERROR_NULL;

    float* dummy = (float*)nimcp_malloc(length * hmm->n_states * sizeof(float));
    if (!dummy) return NIMCP_ML_ERROR_MEMORY;

    nimcp_ml_error_t err = nimcp_hmm_predict(hmm, observations, length, dummy, log_likelihood);
    nimcp_free(dummy);
    return err;
}

void nimcp_hmm_decode_result_free(nimcp_hmm_decode_result_t* result) {
    if (!result) return;
    nimcp_free(result->states);
    nimcp_free(result->state_probs);
    result->states = NULL;
    result->state_probs = NULL;
}

//=============================================================================
// KDE Implementation
//=============================================================================

nimcp_kde_config_t nimcp_kde_default_config(void) {
    nimcp_kde_config_t config = {
        .kernel = NIMCP_KDE_KERNEL_GAUSSIAN,
        .bw_method = NIMCP_KDE_BW_SILVERMAN,
        .bandwidth = 1.0f,
        .bandwidths = NULL,
        .adaptive = false,
        .cv_folds = 5,
        .use_gpu = false
    };
    return config;
}

nimcp_kde_t* nimcp_kde_create(const nimcp_kde_config_t* config) {
    nimcp_kde_config_t cfg = config ? *config : nimcp_kde_default_config();

    nimcp_kde_t* kde = (nimcp_kde_t*)nimcp_calloc(1, sizeof(nimcp_kde_t));
    if (!kde) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_kde_create: kde is NULL");
        return NULL;
    }

    kde->magic = KDE_MAGIC;
    kde->kernel = cfg.kernel;
    kde->adaptive = cfg.adaptive;
    kde->use_gpu = cfg.use_gpu && g_ml_state.gpu_ctx != NULL;
    kde->gpu_ctx = g_ml_state.gpu_ctx;

    return kde;
}

void nimcp_kde_destroy(nimcp_kde_t* kde) {
    if (!kde) return;
    if (kde->magic != KDE_MAGIC) return;

    nimcp_free(kde->data);
    nimcp_free(kde->bandwidth);
    nimcp_free(kde->adaptive_bw);

    kde->magic = 0;
    nimcp_free(kde);
}

nimcp_ml_error_t nimcp_kde_bandwidth_silverman(const float* X, uint32_t n_samples,
                                               uint32_t n_features, float* bandwidth) {
    if (!X || !bandwidth) return NIMCP_ML_ERROR_NULL;

    float factor = powf((float)n_samples, -1.0f / ((float)n_features + 4.0f));

    for (uint32_t j = 0; j < n_features; j++) {
        float mean = 0.0f;
        for (uint32_t i = 0; i < n_samples; i++) {
            mean += X[i * n_features + j];
        }
        mean /= (float)n_samples;

        float var = 0.0f;
        for (uint32_t i = 0; i < n_samples; i++) {
            float diff = X[i * n_features + j] - mean;
            var += diff * diff;
        }
        var /= (float)(n_samples - 1);

        float std = sqrtf(var);
        bandwidth[j] = 1.06f * std * factor;
        if (bandwidth[j] < NIMCP_ML_EPS) bandwidth[j] = 1.0f;
    }

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_kde_bandwidth_scott(const float* X, uint32_t n_samples,
                                           uint32_t n_features, float* bandwidth) {
    if (!X || !bandwidth) return NIMCP_ML_ERROR_NULL;

    float factor = powf((float)n_samples, -1.0f / ((float)n_features + 4.0f));

    for (uint32_t j = 0; j < n_features; j++) {
        float mean = 0.0f;
        for (uint32_t i = 0; i < n_samples; i++) {
            mean += X[i * n_features + j];
        }
        mean /= (float)n_samples;

        float var = 0.0f;
        for (uint32_t i = 0; i < n_samples; i++) {
            float diff = X[i * n_features + j] - mean;
            var += diff * diff;
        }
        var /= (float)(n_samples - 1);

        float std = sqrtf(var);
        bandwidth[j] = std * factor;
        if (bandwidth[j] < NIMCP_ML_EPS) bandwidth[j] = 1.0f;
    }

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_kde_bandwidth_cv(nimcp_kde_t* kde, const float* X,
                                        uint32_t n_samples, uint32_t n_features,
                                        uint32_t n_folds, float* bandwidth) {
    /* Use Scott's rule as starting point */
    return nimcp_kde_bandwidth_scott(X, n_samples, n_features, bandwidth);
}

nimcp_ml_error_t nimcp_kde_fit(nimcp_kde_t* kde, const float* X,
                               uint32_t n_samples, uint32_t n_features) {
    if (!kde || kde->magic != KDE_MAGIC) return NIMCP_ML_ERROR_NULL;
    if (!X || n_samples == 0) return NIMCP_ML_ERROR_PARAMS;

    kde->n_samples = n_samples;
    kde->n_features = n_features;

    kde->data = (float*)nimcp_realloc(kde->data, n_samples * n_features * sizeof(float));
    kde->bandwidth = (float*)nimcp_realloc(kde->bandwidth, n_features * sizeof(float));

    if (!kde->data || !kde->bandwidth) {
        return NIMCP_ML_ERROR_MEMORY;
    }

    memcpy(kde->data, X, n_samples * n_features * sizeof(float));

    /* Compute bandwidth using Silverman's rule */
    nimcp_ml_error_t err = nimcp_kde_bandwidth_silverman(X, n_samples, n_features, kde->bandwidth);
    if (err != NIMCP_ML_OK) return err;

    /* Compute log normalization */
    kde->log_norm = 0.0f;
    for (uint32_t j = 0; j < n_features; j++) {
        kde->log_norm += logf(kde->bandwidth[j]) + 0.5f * logf(2.0f * PI_F);
    }

    kde->is_fitted = true;
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_kde_evaluate(const nimcp_kde_t* kde, const float* X,
                                    uint32_t n_points, float* density) {
    if (!kde || kde->magic != KDE_MAGIC || !kde->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!X || !density) return NIMCP_ML_ERROR_NULL;

    uint32_t n = kde->n_samples;
    uint32_t d = kde->n_features;

    /* Heap-allocate scratch buffer — n_samples can be arbitrarily large,
     * so alloca() would overflow the stack. */
    float* log_vals = (float*)nimcp_malloc(n * sizeof(float));
    if (!log_vals) return NIMCP_ML_ERROR_MEMORY;

    for (uint32_t i = 0; i < n_points; i++) {
        for (uint32_t k = 0; k < n; k++) {
            float log_kern = 0.0f;
            for (uint32_t j = 0; j < d; j++) {
                float diff = (X[i * d + j] - kde->data[k * d + j]) / kde->bandwidth[j];
                log_kern += -0.5f * diff * diff;
            }
            log_vals[k] = log_kern - kde->log_norm;
        }

        density[i] = log_sum_exp(log_vals, n) - logf((float)n);
    }

    nimcp_free(log_vals);
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_kde_sample(const nimcp_kde_t* kde, uint32_t n_samples, float* samples) {
    if (!kde || kde->magic != KDE_MAGIC || !kde->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!samples) return NIMCP_ML_ERROR_NULL;

    uint32_t n = kde->n_samples;
    uint32_t d = kde->n_features;

    for (uint32_t i = 0; i < n_samples; i++) {
        /* Pick random data point */
        uint32_t idx = nimcp_tl_rand() % n;

        /* Add Gaussian noise */
        for (uint32_t j = 0; j < d; j++) {
            samples[i * d + j] = kde->data[idx * d + j] + kde->bandwidth[j] * randn();
        }
    }

    return NIMCP_ML_OK;
}

//=============================================================================
// Naive Bayes Implementation
//=============================================================================

nimcp_nb_config_t nimcp_nb_default_config(nimcp_nb_type_t type) {
    nimcp_nb_config_t config = {
        .type = type,
        .alpha = 1.0f,
        .fit_prior = true,
        .class_prior = NULL,
        .n_classes = 2,
        .var_smoothing = 1e-9f
    };
    return config;
}

nimcp_nb_t* nimcp_nb_create(const nimcp_nb_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_nb_create: config is NULL");
        return NULL;
    }

    nimcp_nb_t* nb = (nimcp_nb_t*)nimcp_calloc(1, sizeof(nimcp_nb_t));
    if (!nb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_nb_create: nb is NULL");
        return NULL;
    }

    nb->magic = NB_MAGIC;
    nb->type = config->type;
    nb->n_classes = config->n_classes;
    nb->alpha = config->alpha;

    return nb;
}

void nimcp_nb_destroy(nimcp_nb_t* nb) {
    if (!nb) return;
    if (nb->magic != NB_MAGIC) return;

    nimcp_free(nb->class_prior);
    nimcp_free(nb->class_count);
    nimcp_free(nb->theta);
    nimcp_free(nb->var);
    nimcp_free(nb->feature_log_prob);
    nimcp_free(nb->feature_count);

    nb->magic = 0;
    nimcp_free(nb);
}

nimcp_ml_error_t nimcp_nb_gaussian_fit(nimcp_nb_t* nb, const float* X,
                                       const uint32_t* y, uint32_t n_samples,
                                       uint32_t n_features) {
    if (!nb || nb->magic != NB_MAGIC) return NIMCP_ML_ERROR_NULL;
    if (!X || !y) return NIMCP_ML_ERROR_NULL;
    if (nb->type != NIMCP_NB_GAUSSIAN) return NIMCP_ML_ERROR_PARAMS;

    /* Find number of classes */
    uint32_t max_class = 0;
    for (uint32_t i = 0; i < n_samples; i++) {
        if (y[i] > max_class) max_class = y[i];
    }
    nb->n_classes = max_class + 1;
    nb->n_features = n_features;

    uint32_t c = nb->n_classes;
    uint32_t d = n_features;

    nb->class_prior = (float*)nimcp_realloc(nb->class_prior, c * sizeof(float));
    nb->class_count = (float*)nimcp_realloc(nb->class_count, c * sizeof(float));
    nb->theta = (float*)nimcp_realloc(nb->theta, c * d * sizeof(float));
    nb->var = (float*)nimcp_realloc(nb->var, c * d * sizeof(float));

    if (!nb->class_prior || !nb->class_count || !nb->theta || !nb->var) {
        return NIMCP_ML_ERROR_MEMORY;
    }

    memset(nb->class_count, 0, c * sizeof(float));
    memset(nb->theta, 0, c * d * sizeof(float));
    memset(nb->var, 0, c * d * sizeof(float));

    /* Count samples per class and sum features */
    for (uint32_t i = 0; i < n_samples; i++) {
        uint32_t cls = y[i];
        nb->class_count[cls] += 1.0f;
        for (uint32_t j = 0; j < d; j++) {
            nb->theta[cls * d + j] += X[i * d + j];
        }
    }

    /* Compute means */
    for (uint32_t k = 0; k < c; k++) {
        float inv_count = 1.0f / (nb->class_count[k] + NIMCP_ML_EPS);
        for (uint32_t j = 0; j < d; j++) {
            nb->theta[k * d + j] *= inv_count;
        }
    }

    /* Compute variances */
    for (uint32_t i = 0; i < n_samples; i++) {
        uint32_t cls = y[i];
        for (uint32_t j = 0; j < d; j++) {
            float diff = X[i * d + j] - nb->theta[cls * d + j];
            nb->var[cls * d + j] += diff * diff;
        }
    }

    for (uint32_t k = 0; k < c; k++) {
        float inv_count = 1.0f / (nb->class_count[k] + NIMCP_ML_EPS);
        for (uint32_t j = 0; j < d; j++) {
            nb->var[k * d + j] = nb->var[k * d + j] * inv_count + 1e-9f;
        }
    }

    /* Compute log class priors */
    for (uint32_t k = 0; k < c; k++) {
        nb->class_prior[k] = safe_log(nb->class_count[k] / (float)n_samples);
    }

    nb->is_fitted = true;
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_nb_gaussian_predict(const nimcp_nb_t* nb, const float* X,
                                           uint32_t n_samples, uint32_t* y_pred) {
    if (!nb || nb->magic != NB_MAGIC || !nb->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!X || !y_pred) return NIMCP_ML_ERROR_NULL;

    uint32_t c = nb->n_classes;
    uint32_t d = nb->n_features;

    for (uint32_t i = 0; i < n_samples; i++) {
        float max_log_prob = -FLT_MAX;
        uint32_t best_class = 0;

        for (uint32_t k = 0; k < c; k++) {
            float log_prob = nb->class_prior[k];

            for (uint32_t j = 0; j < d; j++) {
                float diff = X[i * d + j] - nb->theta[k * d + j];
                log_prob += -0.5f * (logf(2.0f * PI_F * nb->var[k * d + j]) +
                                    diff * diff / nb->var[k * d + j]);
            }

            if (log_prob > max_log_prob) {
                max_log_prob = log_prob;
                best_class = k;
            }
        }

        y_pred[i] = best_class;
    }

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_nb_multinomial_fit(nimcp_nb_t* nb, const float* X,
                                          const uint32_t* y, uint32_t n_samples,
                                          uint32_t n_features) {
    if (!nb || nb->magic != NB_MAGIC) return NIMCP_ML_ERROR_NULL;
    if (!X || !y) return NIMCP_ML_ERROR_NULL;
    if (nb->type != NIMCP_NB_MULTINOMIAL) return NIMCP_ML_ERROR_PARAMS;

    uint32_t max_class = 0;
    for (uint32_t i = 0; i < n_samples; i++) {
        if (y[i] > max_class) max_class = y[i];
    }
    nb->n_classes = max_class + 1;
    nb->n_features = n_features;

    uint32_t c = nb->n_classes;
    uint32_t d = n_features;

    nb->class_prior = (float*)nimcp_realloc(nb->class_prior, c * sizeof(float));
    nb->class_count = (float*)nimcp_realloc(nb->class_count, c * sizeof(float));
    nb->feature_count = (float*)nimcp_realloc(nb->feature_count, c * d * sizeof(float));
    nb->feature_log_prob = (float*)nimcp_realloc(nb->feature_log_prob, c * d * sizeof(float));

    if (!nb->class_prior || !nb->class_count || !nb->feature_count || !nb->feature_log_prob) {
        return NIMCP_ML_ERROR_MEMORY;
    }

    memset(nb->class_count, 0, c * sizeof(float));
    memset(nb->feature_count, 0, c * d * sizeof(float));

    for (uint32_t i = 0; i < n_samples; i++) {
        uint32_t cls = y[i];
        nb->class_count[cls] += 1.0f;
        for (uint32_t j = 0; j < d; j++) {
            nb->feature_count[cls * d + j] += X[i * d + j];
        }
    }

    for (uint32_t k = 0; k < c; k++) {
        nb->class_prior[k] = safe_log(nb->class_count[k] / (float)n_samples);

        float total = nb->alpha * (float)d;
        for (uint32_t j = 0; j < d; j++) {
            total += nb->feature_count[k * d + j];
        }

        for (uint32_t j = 0; j < d; j++) {
            nb->feature_log_prob[k * d + j] = safe_log((nb->feature_count[k * d + j] + nb->alpha) / total);
        }
    }

    nb->is_fitted = true;
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_nb_multinomial_predict(const nimcp_nb_t* nb, const float* X,
                                              uint32_t n_samples, uint32_t* y_pred) {
    if (!nb || nb->magic != NB_MAGIC || !nb->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!X || !y_pred) return NIMCP_ML_ERROR_NULL;

    uint32_t c = nb->n_classes;
    uint32_t d = nb->n_features;

    for (uint32_t i = 0; i < n_samples; i++) {
        float max_log_prob = -FLT_MAX;
        uint32_t best_class = 0;

        for (uint32_t k = 0; k < c; k++) {
            float log_prob = nb->class_prior[k];
            for (uint32_t j = 0; j < d; j++) {
                log_prob += X[i * d + j] * nb->feature_log_prob[k * d + j];
            }

            if (log_prob > max_log_prob) {
                max_log_prob = log_prob;
                best_class = k;
            }
        }

        y_pred[i] = best_class;
    }

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_nb_bernoulli_fit(nimcp_nb_t* nb, const float* X,
                                        const uint32_t* y, uint32_t n_samples,
                                        uint32_t n_features) {
    if (!nb || nb->magic != NB_MAGIC) return NIMCP_ML_ERROR_NULL;
    if (!X || !y) return NIMCP_ML_ERROR_NULL;
    if (nb->type != NIMCP_NB_BERNOULLI) return NIMCP_ML_ERROR_PARAMS;

    uint32_t max_class = 0;
    for (uint32_t i = 0; i < n_samples; i++) {
        if (y[i] > max_class) max_class = y[i];
    }
    nb->n_classes = max_class + 1;
    nb->n_features = n_features;

    uint32_t c = nb->n_classes;
    uint32_t d = n_features;

    nb->class_prior = (float*)nimcp_realloc(nb->class_prior, c * sizeof(float));
    nb->class_count = (float*)nimcp_realloc(nb->class_count, c * sizeof(float));
    nb->feature_count = (float*)nimcp_realloc(nb->feature_count, c * d * sizeof(float));
    nb->feature_log_prob = (float*)nimcp_realloc(nb->feature_log_prob, c * d * sizeof(float));

    if (!nb->class_prior || !nb->class_count || !nb->feature_count || !nb->feature_log_prob) {
        return NIMCP_ML_ERROR_MEMORY;
    }

    memset(nb->class_count, 0, c * sizeof(float));
    memset(nb->feature_count, 0, c * d * sizeof(float));

    for (uint32_t i = 0; i < n_samples; i++) {
        uint32_t cls = y[i];
        nb->class_count[cls] += 1.0f;
        for (uint32_t j = 0; j < d; j++) {
            if (X[i * d + j] > 0.5f) {
                nb->feature_count[cls * d + j] += 1.0f;
            }
        }
    }

    for (uint32_t k = 0; k < c; k++) {
        nb->class_prior[k] = safe_log(nb->class_count[k] / (float)n_samples);

        for (uint32_t j = 0; j < d; j++) {
            float p = (nb->feature_count[k * d + j] + nb->alpha) /
                      (nb->class_count[k] + 2.0f * nb->alpha);
            nb->feature_log_prob[k * d + j] = safe_log(p);
        }
    }

    nb->is_fitted = true;
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_nb_bernoulli_predict(const nimcp_nb_t* nb, const float* X,
                                            uint32_t n_samples, uint32_t* y_pred) {
    if (!nb || nb->magic != NB_MAGIC || !nb->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!X || !y_pred) return NIMCP_ML_ERROR_NULL;

    uint32_t c = nb->n_classes;
    uint32_t d = nb->n_features;

    for (uint32_t i = 0; i < n_samples; i++) {
        float max_log_prob = -FLT_MAX;
        uint32_t best_class = 0;

        for (uint32_t k = 0; k < c; k++) {
            float log_prob = nb->class_prior[k];
            for (uint32_t j = 0; j < d; j++) {
                float log_p = nb->feature_log_prob[k * d + j];
                float log_1_p = safe_log(1.0f - expf(log_p));

                if (X[i * d + j] > 0.5f) {
                    log_prob += log_p;
                } else {
                    log_prob += log_1_p;
                }
            }

            if (log_prob > max_log_prob) {
                max_log_prob = log_prob;
                best_class = k;
            }
        }

        y_pred[i] = best_class;
    }

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_nb_predict_proba(const nimcp_nb_t* nb, const float* X,
                                        uint32_t n_samples, float* probs) {
    if (!nb || nb->magic != NB_MAGIC || !nb->is_fitted) return NIMCP_ML_ERROR_NOT_FITTED;
    if (!X || !probs) return NIMCP_ML_ERROR_NULL;

    uint32_t c = nb->n_classes;
    uint32_t d = nb->n_features;

    /* Heap-allocate to avoid stack overflow with large class counts */
    float* log_probs = (float*)nimcp_malloc(c * sizeof(float));
    if (!log_probs) return NIMCP_ML_ERROR_MEMORY;

    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t k = 0; k < c; k++) {
            float log_prob = nb->class_prior[k];

            if (nb->type == NIMCP_NB_GAUSSIAN) {
                for (uint32_t j = 0; j < d; j++) {
                    float diff = X[i * d + j] - nb->theta[k * d + j];
                    log_prob += -0.5f * (logf(2.0f * PI_F * nb->var[k * d + j]) +
                                        diff * diff / nb->var[k * d + j]);
                }
            } else if (nb->type == NIMCP_NB_MULTINOMIAL) {
                for (uint32_t j = 0; j < d; j++) {
                    log_prob += X[i * d + j] * nb->feature_log_prob[k * d + j];
                }
            } else {
                for (uint32_t j = 0; j < d; j++) {
                    float log_p = nb->feature_log_prob[k * d + j];
                    float log_1_p = safe_log(1.0f - expf(log_p));
                    if (X[i * d + j] > 0.5f) {
                        log_prob += log_p;
                    } else {
                        log_prob += log_1_p;
                    }
                }
            }

            log_probs[k] = log_prob;
        }

        float log_sum = log_sum_exp(log_probs, c);
        for (uint32_t k = 0; k < c; k++) {
            probs[i * c + k] = expf(log_probs[k] - log_sum);
        }
    }

    nimcp_free(log_probs);
    return NIMCP_ML_OK;
}

//=============================================================================
// Model Evaluation Implementation
//=============================================================================

nimcp_ml_error_t nimcp_ml_confusion_matrix(const uint32_t* y_true, const uint32_t* y_pred,
                                           uint32_t n_samples, uint32_t n_classes,
                                           nimcp_confusion_matrix_t* cm) {
    if (!y_true || !y_pred || !cm) return NIMCP_ML_ERROR_NULL;

    cm->n_classes = n_classes;
    cm->total = n_samples;

    cm->matrix = (uint32_t*)nimcp_calloc(n_classes * n_classes, sizeof(uint32_t));
    cm->precision = (float*)nimcp_calloc(n_classes, sizeof(float));
    cm->recall = (float*)nimcp_calloc(n_classes, sizeof(float));
    cm->f1 = (float*)nimcp_calloc(n_classes, sizeof(float));

    if (!cm->matrix || !cm->precision || !cm->recall || !cm->f1) {
        nimcp_ml_confusion_matrix_free(cm);
        return NIMCP_ML_ERROR_MEMORY;
    }

    /* Build confusion matrix */
    uint32_t correct = 0;
    for (uint32_t i = 0; i < n_samples; i++) {
        cm->matrix[y_true[i] * n_classes + y_pred[i]]++;
        if (y_true[i] == y_pred[i]) correct++;
    }

    cm->accuracy = (float)correct / (float)n_samples;

    /* Compute per-class metrics */
    float macro_f1 = 0.0f;
    float weighted_f1 = 0.0f;
    uint32_t total_support = 0;

    for (uint32_t k = 0; k < n_classes; k++) {
        uint32_t tp = cm->matrix[k * n_classes + k];
        uint32_t pred_sum = 0, true_sum = 0;

        for (uint32_t j = 0; j < n_classes; j++) {
            pred_sum += cm->matrix[j * n_classes + k];
            true_sum += cm->matrix[k * n_classes + j];
        }

        cm->precision[k] = pred_sum > 0 ? (float)tp / (float)pred_sum : 0.0f;
        cm->recall[k] = true_sum > 0 ? (float)tp / (float)true_sum : 0.0f;

        if (cm->precision[k] + cm->recall[k] > 0) {
            cm->f1[k] = 2.0f * cm->precision[k] * cm->recall[k] /
                        (cm->precision[k] + cm->recall[k]);
        } else {
            cm->f1[k] = 0.0f;
        }

        macro_f1 += cm->f1[k];
        weighted_f1 += cm->f1[k] * (float)true_sum;
        total_support += true_sum;
    }

    cm->macro_f1 = macro_f1 / (float)n_classes;
    cm->weighted_f1 = total_support > 0 ? weighted_f1 / (float)total_support : 0.0f;

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_ml_roc_curve(const uint32_t* y_true, const float* y_score,
                                    uint32_t n_samples, nimcp_roc_curve_t* roc) {
    if (!y_true || !y_score || !roc) return NIMCP_ML_ERROR_NULL;

    /* Count positives and negatives */
    uint32_t n_pos = 0, n_neg = 0;
    for (uint32_t i = 0; i < n_samples; i++) {
        if (y_true[i] == 1) n_pos++;
        else n_neg++;
    }

    if (n_pos == 0 || n_neg == 0) {
        return NIMCP_ML_ERROR_PARAMS;
    }

    /* Sort indices by score descending */
    uint32_t* indices = (uint32_t*)nimcp_malloc(n_samples * sizeof(uint32_t));
    if (!indices) return NIMCP_ML_ERROR_MEMORY;

    for (uint32_t i = 0; i < n_samples; i++) indices[i] = i;

    /* Simple bubble sort (for simplicity) */
    for (uint32_t i = 0; i < n_samples - 1; i++) {
        for (uint32_t j = 0; j < n_samples - i - 1; j++) {
            if (y_score[indices[j]] < y_score[indices[j+1]]) {
                uint32_t tmp = indices[j];
                indices[j] = indices[j+1];
                indices[j+1] = tmp;
            }
        }
    }

    /* Compute ROC curve */
    roc->n_points = n_samples + 1;
    roc->fpr = (float*)nimcp_malloc(roc->n_points * sizeof(float));
    roc->tpr = (float*)nimcp_malloc(roc->n_points * sizeof(float));
    roc->thresholds = (float*)nimcp_malloc(roc->n_points * sizeof(float));

    if (!roc->fpr || !roc->tpr || !roc->thresholds) {
        nimcp_free(indices);
        nimcp_ml_roc_curve_free(roc);
        return NIMCP_ML_ERROR_MEMORY;
    }

    roc->fpr[0] = 0.0f;
    roc->tpr[0] = 0.0f;
    roc->thresholds[0] = y_score[indices[0]] + 1.0f;

    uint32_t tp = 0, fp = 0;
    for (uint32_t i = 0; i < n_samples; i++) {
        if (y_true[indices[i]] == 1) tp++;
        else fp++;

        roc->tpr[i + 1] = (float)tp / (float)n_pos;
        roc->fpr[i + 1] = (float)fp / (float)n_neg;
        roc->thresholds[i + 1] = y_score[indices[i]];
    }

    /* Compute AUC using trapezoidal rule */
    roc->auc = 0.0f;
    for (uint32_t i = 1; i < roc->n_points; i++) {
        roc->auc += 0.5f * (roc->fpr[i] - roc->fpr[i-1]) * (roc->tpr[i] + roc->tpr[i-1]);
    }

    nimcp_free(indices);
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_ml_auc(const uint32_t* y_true, const float* y_score,
                              uint32_t n_samples, float* auc) {
    nimcp_roc_curve_t roc = {0};
    nimcp_ml_error_t err = nimcp_ml_roc_curve(y_true, y_score, n_samples, &roc);
    if (err != NIMCP_ML_OK) return err;

    *auc = roc.auc;
    nimcp_ml_roc_curve_free(&roc);
    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_ml_precision_recall(const uint32_t* y_true, const float* y_score,
                                           uint32_t n_samples, uint32_t n_points,
                                           float* precision, float* recall) {
    if (!y_true || !y_score || !precision || !recall) return NIMCP_ML_ERROR_NULL;

    uint32_t n_pos = 0;
    for (uint32_t i = 0; i < n_samples; i++) {
        if (y_true[i] == 1) n_pos++;
    }

    if (n_pos == 0) return NIMCP_ML_ERROR_PARAMS;

    /* Compute at different thresholds */
    for (uint32_t p = 0; p < n_points; p++) {
        float threshold = (float)p / (float)(n_points - 1);

        uint32_t tp = 0, fp = 0;
        for (uint32_t i = 0; i < n_samples; i++) {
            if (y_score[i] >= threshold) {
                if (y_true[i] == 1) tp++;
                else fp++;
            }
        }

        precision[p] = (tp + fp > 0) ? (float)tp / (float)(tp + fp) : 1.0f;
        recall[p] = (float)tp / (float)n_pos;
    }

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_ml_f1_score(const uint32_t* y_true, const uint32_t* y_pred,
                                   uint32_t n_samples, float beta, float* f_score) {
    if (!y_true || !y_pred || !f_score) return NIMCP_ML_ERROR_NULL;

    uint32_t tp = 0, fp = 0, fn = 0;
    for (uint32_t i = 0; i < n_samples; i++) {
        if (y_true[i] == 1 && y_pred[i] == 1) tp++;
        else if (y_true[i] == 0 && y_pred[i] == 1) fp++;
        else if (y_true[i] == 1 && y_pred[i] == 0) fn++;
    }

    float precision = (tp + fp > 0) ? (float)tp / (float)(tp + fp) : 0.0f;
    float recall = (tp + fn > 0) ? (float)tp / (float)(tp + fn) : 0.0f;

    float beta2 = beta * beta;
    if (precision + recall > 0) {
        *f_score = (1.0f + beta2) * precision * recall / (beta2 * precision + recall);
    } else {
        *f_score = 0.0f;
    }

    return NIMCP_ML_OK;
}

nimcp_ml_error_t nimcp_ml_cross_validate(const float* X, const uint32_t* y,
                                         uint32_t n_samples, uint32_t n_features,
                                         uint32_t n_folds, const char* model_type,
                                         const void* model_config, nimcp_cv_result_t* result) {
    if (!X || !y || !result) return NIMCP_ML_ERROR_NULL;
    if (n_folds < 2 || n_folds > n_samples) return NIMCP_ML_ERROR_PARAMS;

    result->n_folds = n_folds;
    result->scores = (float*)nimcp_calloc(n_folds, sizeof(float));
    result->train_scores = (float*)nimcp_calloc(n_folds, sizeof(float));

    if (!result->scores || !result->train_scores) {
        nimcp_ml_cv_result_free(result);
        return NIMCP_ML_ERROR_MEMORY;
    }

    uint32_t fold_size = n_samples / n_folds;

    for (uint32_t fold = 0; fold < n_folds; fold++) {
        uint32_t test_start = fold * fold_size;
        uint32_t test_end = (fold == n_folds - 1) ? n_samples : (fold + 1) * fold_size;
        uint32_t n_test = test_end - test_start;
        uint32_t n_train = n_samples - n_test;

        /* Create train/test split */
        float* X_train = (float*)nimcp_malloc(n_train * n_features * sizeof(float));
        uint32_t* y_train = (uint32_t*)nimcp_malloc(n_train * sizeof(uint32_t));
        float* X_test = (float*)nimcp_malloc(n_test * n_features * sizeof(float));
        uint32_t* y_test = (uint32_t*)nimcp_malloc(n_test * sizeof(uint32_t));
        uint32_t* y_pred = (uint32_t*)nimcp_malloc(n_test * sizeof(uint32_t));

        if (!X_train || !y_train || !X_test || !y_test || !y_pred) {
            nimcp_free(X_train); nimcp_free(y_train); nimcp_free(X_test); nimcp_free(y_test); nimcp_free(y_pred);
            nimcp_ml_cv_result_free(result);
            return NIMCP_ML_ERROR_MEMORY;
        }

        uint32_t train_idx = 0;
        for (uint32_t i = 0; i < n_samples; i++) {
            if (i < test_start || i >= test_end) {
                memcpy(&X_train[train_idx * n_features], &X[i * n_features],
                       n_features * sizeof(float));
                y_train[train_idx] = y[i];
                train_idx++;
            }
        }

        memcpy(X_test, &X[test_start * n_features], n_test * n_features * sizeof(float));
        memcpy(y_test, &y[test_start], n_test * sizeof(uint32_t));

        /* Fit and predict using Gaussian NB as default */
        nimcp_nb_config_t nb_config = nimcp_nb_default_config(NIMCP_NB_GAUSSIAN);
        nimcp_nb_t* nb = nimcp_nb_create(&nb_config);
        if (nb) {
            nimcp_nb_gaussian_fit(nb, X_train, y_train, n_train, n_features);
            nimcp_nb_gaussian_predict(nb, X_test, n_test, y_pred);

            /* Compute accuracy */
            uint32_t correct = 0;
            for (uint32_t i = 0; i < n_test; i++) {
                if (y_pred[i] == y_test[i]) correct++;
            }
            result->scores[fold] = (float)correct / (float)n_test;

            nimcp_nb_destroy(nb);
        }

        nimcp_free(X_train); nimcp_free(y_train); nimcp_free(X_test); nimcp_free(y_test); nimcp_free(y_pred);
    }

    /* Compute mean and std */
    result->mean_score = 0.0f;
    for (uint32_t i = 0; i < n_folds; i++) {
        result->mean_score += result->scores[i];
    }
    result->mean_score /= (float)n_folds;

    result->std_score = 0.0f;
    for (uint32_t i = 0; i < n_folds; i++) {
        float diff = result->scores[i] - result->mean_score;
        result->std_score += diff * diff;
    }
    result->std_score = sqrtf(result->std_score / (float)n_folds);

    return NIMCP_ML_OK;
}

void nimcp_ml_confusion_matrix_free(nimcp_confusion_matrix_t* cm) {
    if (!cm) return;
    nimcp_free(cm->matrix);
    nimcp_free(cm->precision);
    nimcp_free(cm->recall);
    nimcp_free(cm->f1);
    memset(cm, 0, sizeof(*cm));
}

void nimcp_ml_roc_curve_free(nimcp_roc_curve_t* roc) {
    if (!roc) return;
    nimcp_free(roc->fpr);
    nimcp_free(roc->tpr);
    nimcp_free(roc->thresholds);
    memset(roc, 0, sizeof(*roc));
}

void nimcp_ml_cv_result_free(nimcp_cv_result_t* result) {
    if (!result) return;
    nimcp_free(result->scores);
    nimcp_free(result->train_scores);
    memset(result, 0, sizeof(*result));
}

//=============================================================================
// GPU Context Management
//=============================================================================

void nimcp_ml_set_gpu_context(nimcp_gpu_context_t* ctx) {
    g_ml_state.gpu_ctx = ctx;
}

nimcp_gpu_context_t* nimcp_ml_get_gpu_context(void) {
    return g_ml_state.gpu_ctx;
}

bool nimcp_ml_gpu_available(void) {
    return g_ml_state.gpu_ctx != NULL;
}

//=============================================================================
// Module Lifecycle
//=============================================================================

nimcp_ml_error_t nimcp_ml_statistics_init(nimcp_gpu_context_t* gpu_ctx) {
    if (g_ml_state.initialized) {
        return NIMCP_ML_OK;
    }

    g_ml_state.gpu_ctx = gpu_ctx;
    g_ml_state.initialized = true;

    LOG_INFO("ML Statistics module initialized (GPU: %s)",
             gpu_ctx ? "enabled" : "disabled");

    return NIMCP_ML_OK;
}

void nimcp_ml_statistics_shutdown(void) {
    if (!g_ml_state.initialized) return;

    g_ml_state.gpu_ctx = NULL;
    g_ml_state.initialized = false;

    LOG_INFO("ML Statistics module shutdown");
}
