/**
 * @file nimcp_vae_loss.c
 * @brief VAE Loss Computation Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * BIO_MODULE: 0x1F04
 */

#include "cognitive/vae/nimcp_vae_loss.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
/* TODO: Fix immune path #include "immune/nimcp_immune.h" */
#include "utils/fault_tolerance/nimcp_health_agent.h"

#include <math.h>
#include <float.h>
#include <string.h>
#include "utils/math/nimcp_math_helpers.h"

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define VAE_LOSS_MODULE_ID      0x1F04
#define VAE_LOSS_EMA_ALPHA      0.99f
#define VAE_LOSS_ACTIVE_THRESHOLD 0.01f

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Safe log with epsilon
 */
static inline float safe_log(float x)
{
    return logf(fmaxf(x, VAE_LOSS_LOG_EPSILON));
}

/**
 * @brief Check if value is invalid (NaN or Inf)
 */
static inline bool is_invalid(float x)
{
    return isnan(x) || isinf(x);
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

vae_loss_config_t vae_loss_default_config(void)
{
    vae_loss_config_t config;
    memset(&config, 0, sizeof(config));

    config.recon_type = VAE_RECON_MSE;
    config.recon_weight = 1.0f;
    config.beta = VAE_LOSS_DEFAULT_BETA;
    config.free_bits = VAE_LOSS_DEFAULT_FREE_BITS;
    config.kl_reduction = VAE_KL_SUM;
    config.aggregation = VAE_LOSS_AGG_MEAN;
    config.huber_delta = 1.0f;
    config.use_precision_weighting = false;
    config.min_precision = 0.01f;
    config.max_precision = 100.0f;
    config.use_kl_annealing = false;
    config.kl_anneal_start = 0.0f;
    config.kl_anneal_end = 1.0f;
    config.kl_anneal_steps = 10000;
    config.use_cyclical_annealing = false;
    config.cycle_steps = 5000;
    config.num_cycles = 4;

    return config;
}

vae_loss_ctx_t* vae_loss_ctx_create(const vae_loss_config_t* config)
{
    if (!config) {
        NIMCP_LOG_ERROR("VAE Loss: NULL config");
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_INVALID_PARAM,
                             "NULL config in vae_loss_ctx_create");
        return NULL;
    }

    vae_loss_ctx_t* ctx = nimcp_calloc(1, sizeof(vae_loss_ctx_t));
    if (!ctx) {
        NIMCP_LOG_ERROR("VAE Loss: Failed to allocate context");
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_VAE_NO_MEMORY,
                             "Failed to allocate loss context");
        return NULL;
    }

    ctx->config = *config;
    ctx->step = 0;
    ctx->current_kl_weight = config->use_kl_annealing ? config->kl_anneal_start : config->beta;
    ctx->ema_recon = 0.0f;
    ctx->ema_kl = 0.0f;
    ctx->ema_alpha = VAE_LOSS_EMA_ALPHA;
    ctx->loss_computations = 0;
    ctx->nan_count = 0;
    ctx->inf_count = 0;

    /* Create mutex */
    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    ctx->mutex = nimcp_mutex_create(&attr);
    if (!ctx->mutex) {
        NIMCP_LOG_ERROR("VAE Loss: Failed to create mutex");
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_loss_ctx_create: ctx->mutex is NULL");
        return NULL;
    }

    ctx->is_initialized = true;

    NIMCP_LOG_INFO("VAE Loss: Context created (recon=%d, beta=%.3f)",
                   config->recon_type, config->beta);

    return ctx;
}

void vae_loss_ctx_destroy(vae_loss_ctx_t* ctx)
{
    if (!ctx) return;

    if (ctx->mutex) {
        nimcp_mutex_destroy(ctx->mutex);
    }

    nimcp_free(ctx);
    ctx = NULL;

    NIMCP_LOG_DEBUG("VAE Loss: Context destroyed");
}

int vae_loss_ctx_reset(vae_loss_ctx_t* ctx)
{
    if (!ctx || !ctx->is_initialized) {
        NIMCP_LOG_ERROR("VAE Loss: Invalid context for reset");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_loss_ctx_reset: required parameter is NULL (ctx, ctx->is_initialized)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    ctx->step = 0;
    ctx->current_kl_weight = ctx->config.use_kl_annealing ?
                             ctx->config.kl_anneal_start : ctx->config.beta;
    ctx->ema_recon = 0.0f;
    ctx->ema_kl = 0.0f;
    ctx->loss_computations = 0;
    ctx->nan_count = 0;
    ctx->inf_count = 0;

    nimcp_mutex_unlock(ctx->mutex);

    NIMCP_LOG_DEBUG("VAE Loss: Context reset");
    return 0;
}

/* ============================================================================
 * Reconstruction Loss Implementation
 * ============================================================================ */

float vae_loss_mse(const nimcp_tensor_t* x,
                   const nimcp_tensor_t* x_recon,
                   vae_loss_aggregation_t reduction)
{
    if (!x || !x_recon || !x->data || !x_recon->data) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_INVALID_PARAM,
                             "NULL tensor in MSE loss");
        return NAN;
    }

    uint32_t total = nimcp_tensor_numel(x);
    if (total == 0 || total != nimcp_tensor_numel(x_recon)) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_INVALID_PARAM,
                             "Tensor size mismatch in MSE loss");
        return NAN;
    }

    const float* x_data = (const float*)x->data;
    const float* recon_data = (const float*)x_recon->data;

    float sum = 0.0f;
    for (uint32_t i = 0; i < total; i++) {
        float diff = x_data[i] - recon_data[i];
        sum += diff * diff;
    }

    uint32_t batch_size = (x->shape.rank >= 1) ? x->shape.dims[0] : 1;

    if (reduction == VAE_LOSS_AGG_MEAN) {
        return sum / (float)total;
    }
    return sum / (float)batch_size;
}

float vae_loss_bce(const nimcp_tensor_t* x,
                   const nimcp_tensor_t* x_recon,
                   vae_loss_aggregation_t reduction)
{
    if (!x || !x_recon || !x->data || !x_recon->data) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_INVALID_PARAM,
                             "NULL tensor in BCE loss");
        return NAN;
    }

    uint32_t total = nimcp_tensor_numel(x);
    if (total == 0 || total != nimcp_tensor_numel(x_recon)) {
        return NAN;
    }

    const float* x_data = (const float*)x->data;
    const float* recon_data = (const float*)x_recon->data;

    float sum = 0.0f;
    for (uint32_t i = 0; i < total; i++) {
        float target = nimcp_clampf(x_data[i], 0.0f, 1.0f);
        float pred = nimcp_clampf(recon_data[i], VAE_LOSS_LOG_EPSILON, 1.0f - VAE_LOSS_LOG_EPSILON);

        sum -= target * safe_log(pred) + (1.0f - target) * safe_log(1.0f - pred);
    }

    uint32_t batch_size = (x->shape.rank >= 1) ? x->shape.dims[0] : 1;

    if (reduction == VAE_LOSS_AGG_MEAN) {
        return sum / (float)total;
    }
    return sum / (float)batch_size;
}

float vae_loss_gaussian_nll(const nimcp_tensor_t* x,
                            const nimcp_tensor_t* recon_mu,
                            const nimcp_tensor_t* recon_log_var,
                            vae_loss_aggregation_t reduction)
{
    if (!x || !recon_mu || !x->data || !recon_mu->data) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_INVALID_PARAM,
                             "NULL tensor in Gaussian NLL loss");
        return NAN;
    }

    uint32_t total = nimcp_tensor_numel(x);
    if (total == 0) return NAN;

    const float* x_data = (const float*)x->data;
    const float* mu_data = (const float*)recon_mu->data;

    float sum = 0.0f;

    if (recon_log_var && recon_log_var->data) {
        /* Heteroscedastic case */
        const float* log_var_data = (const float*)recon_log_var->data;

        for (uint32_t i = 0; i < total; i++) {
            float log_var = nimcp_clampf(log_var_data[i], -10.0f, 10.0f);
            float var = expf(log_var);
            var = fmaxf(var, VAE_LOSS_MIN_VAR);

            float diff = x_data[i] - mu_data[i];
            sum += 0.5f * (log_var + (diff * diff) / var);
        }
    } else {
        /* Homoscedastic case (unit variance) */
        for (uint32_t i = 0; i < total; i++) {
            float diff = x_data[i] - mu_data[i];
            sum += 0.5f * diff * diff;
        }
    }

    uint32_t batch_size = (x->shape.rank >= 1) ? x->shape.dims[0] : 1;

    if (reduction == VAE_LOSS_AGG_MEAN) {
        return sum / (float)total;
    }
    return sum / (float)batch_size;
}

float vae_loss_laplace_nll(const nimcp_tensor_t* x,
                           const nimcp_tensor_t* recon_mu,
                           float scale,
                           vae_loss_aggregation_t reduction)
{
    if (!x || !recon_mu || !x->data || !recon_mu->data) {
        return NAN;
    }

    uint32_t total = nimcp_tensor_numel(x);
    if (total == 0) return NAN;

    scale = fmaxf(scale, VAE_LOSS_LOG_EPSILON);

    const float* x_data = (const float*)x->data;
    const float* mu_data = (const float*)recon_mu->data;

    float sum = 0.0f;
    float log_2b = logf(2.0f * scale);

    for (uint32_t i = 0; i < total; i++) {
        float diff = fabsf(x_data[i] - mu_data[i]);
        sum += log_2b + diff / (fabsf(scale) > 1e-7f ? scale : 1e-7f);
    }

    uint32_t batch_size = (x->shape.rank >= 1) ? x->shape.dims[0] : 1;

    if (reduction == VAE_LOSS_AGG_MEAN) {
        return sum / (float)total;
    }
    return sum / (float)batch_size;
}

float vae_loss_huber(const nimcp_tensor_t* x,
                     const nimcp_tensor_t* x_recon,
                     float delta,
                     vae_loss_aggregation_t reduction)
{
    if (!x || !x_recon || !x->data || !x_recon->data) {
        return NAN;
    }

    uint32_t total = nimcp_tensor_numel(x);
    if (total == 0) return NAN;

    delta = fmaxf(delta, VAE_LOSS_LOG_EPSILON);

    const float* x_data = (const float*)x->data;
    const float* recon_data = (const float*)x_recon->data;

    float sum = 0.0f;

    for (uint32_t i = 0; i < total; i++) {
        float diff = fabsf(x_data[i] - recon_data[i]);

        if (diff <= delta) {
            sum += 0.5f * diff * diff;
        } else {
            sum += delta * (diff - 0.5f * delta);
        }
    }

    uint32_t batch_size = (x->shape.rank >= 1) ? x->shape.dims[0] : 1;

    if (reduction == VAE_LOSS_AGG_MEAN) {
        return sum / (float)total;
    }
    return sum / (float)batch_size;
}

/* ============================================================================
 * KL Divergence Implementation
 * ============================================================================ */

float vae_loss_kl_standard_normal(const nimcp_tensor_t* mu,
                                  const nimcp_tensor_t* log_var,
                                  vae_kl_reduction_t reduction,
                                  vae_loss_aggregation_t batch_reduction)
{
    if (!mu || !log_var || !mu->data || !log_var->data) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_INVALID_PARAM,
                             "NULL tensor in KL divergence");
        return NAN;
    }

    uint32_t total = nimcp_tensor_numel(mu);
    if (total == 0 || total != nimcp_tensor_numel(log_var)) {
        return NAN;
    }

    const float* mu_data = (const float*)mu->data;
    const float* log_var_data = (const float*)log_var->data;

    /* Determine dimensions */
    uint32_t batch_size = (mu->shape.rank >= 1) ? mu->shape.dims[0] : 1;
    uint32_t latent_dim = total / batch_size;

    float kl_sum = 0.0f;

    /* KL[N(mu, sigma^2) || N(0,I)] = -0.5 * sum(1 + log_var - mu^2 - exp(log_var)) */
    for (uint32_t i = 0; i < total; i++) {
        float lv = nimcp_clampf(log_var_data[i], -10.0f, 10.0f);
        float kl_i = -0.5f * (1.0f + lv - mu_data[i] * mu_data[i] - expf(lv));
        kl_sum += kl_i;
    }

    /* Apply reductions */
    float result = kl_sum;

    if (reduction == VAE_KL_MEAN) {
        result /= (float)latent_dim;
    }

    if (batch_reduction == VAE_LOSS_AGG_MEAN) {
        result /= (float)batch_size;
    }

    return result;
}

float vae_loss_kl_with_free_bits(const nimcp_tensor_t* mu,
                                 const nimcp_tensor_t* log_var,
                                 float free_bits,
                                 float* kl_per_dim,
                                 vae_loss_aggregation_t batch_reduction)
{
    if (!mu || !log_var || !mu->data || !log_var->data) {
        return NAN;
    }

    uint32_t total = nimcp_tensor_numel(mu);
    uint32_t batch_size = (mu->shape.rank >= 1) ? mu->shape.dims[0] : 1;
    uint32_t latent_dim = total / batch_size;

    const float* mu_data = (const float*)mu->data;
    const float* log_var_data = (const float*)log_var->data;

    /* Compute per-dimension KL (averaged over batch) */
    float* dim_kl = kl_per_dim;
    float local_dim_kl[256];

    if (!dim_kl && latent_dim <= 256) {
        dim_kl = local_dim_kl;
    }

    if (dim_kl) {
        memset(dim_kl, 0, latent_dim * sizeof(float));

        for (uint32_t b = 0; b < batch_size; b++) {
            for (uint32_t d = 0; d < latent_dim; d++) {
                uint32_t idx = b * latent_dim + d;
                float lv = nimcp_clampf(log_var_data[idx], -10.0f, 10.0f);
                float kl_i = -0.5f * (1.0f + lv - mu_data[idx] * mu_data[idx] - expf(lv));
                dim_kl[d] += kl_i;
            }
        }

        /* Average over batch */
        for (uint32_t d = 0; d < latent_dim; d++) {
            dim_kl[d] /= (float)batch_size;
        }
    }

    /* Apply free bits and sum */
    float kl_sum = 0.0f;

    if (dim_kl) {
        for (uint32_t d = 0; d < latent_dim; d++) {
            float kl_d = fmaxf(free_bits, dim_kl[d]);
            kl_sum += kl_d;
        }
    } else {
        /* Fallback: compute directly without per-dim tracking */
        for (uint32_t i = 0; i < total; i++) {
            float lv = nimcp_clampf(log_var_data[i], -10.0f, 10.0f);
            float kl_i = -0.5f * (1.0f + lv - mu_data[i] * mu_data[i] - expf(lv));
            kl_sum += fmaxf(free_bits, kl_i);
        }

        if (batch_reduction == VAE_LOSS_AGG_MEAN) {
            kl_sum /= (float)batch_size;
        }
    }

    return kl_sum;
}

int vae_loss_kl_per_dimension(const nimcp_tensor_t* mu,
                              const nimcp_tensor_t* log_var,
                              float* kl_per_dim)
{
    if (!mu || !log_var || !kl_per_dim || !mu->data || !log_var->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_loss_kl_per_dimension: required parameter is NULL (mu, log_var, kl_per_dim, mu->data, log_var->data)");
        return -1;
    }

    uint32_t total = nimcp_tensor_numel(mu);
    uint32_t batch_size = (mu->shape.rank >= 1) ? mu->shape.dims[0] : 1;
    uint32_t latent_dim = total / batch_size;

    const float* mu_data = (const float*)mu->data;
    const float* log_var_data = (const float*)log_var->data;

    memset(kl_per_dim, 0, latent_dim * sizeof(float));

    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t d = 0; d < latent_dim; d++) {
            uint32_t idx = b * latent_dim + d;
            float lv = nimcp_clampf(log_var_data[idx], -10.0f, 10.0f);
            float kl_i = -0.5f * (1.0f + lv - mu_data[idx] * mu_data[idx] - expf(lv));
            kl_per_dim[d] += kl_i;
        }
    }

    /* Average over batch */
    for (uint32_t d = 0; d < latent_dim; d++) {
        kl_per_dim[d] /= (float)batch_size;
    }

    return 0;
}

/* ============================================================================
 * Core Loss Computation
 * ============================================================================ */

float vae_loss_compute(vae_loss_ctx_t* ctx,
                       const nimcp_tensor_t* x,
                       const nimcp_tensor_t* x_recon,
                       const nimcp_tensor_t* mu,
                       const nimcp_tensor_t* log_var,
                       vae_loss_breakdown_t* breakdown)
{
    if (!ctx || !ctx->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_INVALID_PARAM,
                             "Invalid loss context");
        return NAN;
    }

    if (!x || !x_recon || !mu || !log_var) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_INVALID_PARAM,
                             "NULL tensor in loss computation");
        return NAN;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Compute reconstruction loss */
    float recon_loss = 0.0f;

    switch (ctx->config.recon_type) {
        case VAE_RECON_MSE:
            recon_loss = vae_loss_mse(x, x_recon, ctx->config.aggregation);
            break;
        case VAE_RECON_BCE:
            recon_loss = vae_loss_bce(x, x_recon, ctx->config.aggregation);
            break;
        case VAE_RECON_GAUSSIAN_NLL:
            recon_loss = vae_loss_gaussian_nll(x, x_recon, NULL, ctx->config.aggregation);
            break;
        case VAE_RECON_LAPLACE_NLL:
            recon_loss = vae_loss_laplace_nll(x, x_recon, 1.0f, ctx->config.aggregation);
            break;
        case VAE_RECON_HUBER:
            recon_loss = vae_loss_huber(x, x_recon, ctx->config.huber_delta, ctx->config.aggregation);
            break;
        default:
            recon_loss = vae_loss_mse(x, x_recon, ctx->config.aggregation);
    }

    recon_loss *= ctx->config.recon_weight;

    /* Compute KL divergence */
    float kl_raw = 0.0f;
    float* kl_per_dim = NULL;

    if (breakdown && breakdown->kl_per_dim && breakdown->latent_dim > 0) {
        kl_per_dim = breakdown->kl_per_dim;
    }

    if (ctx->config.free_bits > 0.0f) {
        kl_raw = vae_loss_kl_with_free_bits(mu, log_var, ctx->config.free_bits,
                                            kl_per_dim, ctx->config.aggregation);
    } else {
        kl_raw = vae_loss_kl_standard_normal(mu, log_var, ctx->config.kl_reduction,
                                             ctx->config.aggregation);

        if (kl_per_dim) {
            vae_loss_kl_per_dimension(mu, log_var, kl_per_dim);
        }
    }

    /* Apply β weighting */
    float kl_weighted = ctx->current_kl_weight * kl_raw;

    /* Compute total loss (negative ELBO) */
    float total_loss = recon_loss + kl_weighted;

    /* Check for numerical issues — skip EMA update if invalid to avoid corruption */
    if (is_invalid(total_loss) || is_invalid(recon_loss) || is_invalid(kl_raw)) {
        ctx->nan_count += isnan(total_loss) ? 1 : 0;
        ctx->inf_count += isinf(total_loss) ? 1 : 0;

        NIMCP_LOG_WARN("VAE Loss: Invalid loss detected (recon=%.4f, kl=%.4f)",
                       recon_loss, kl_raw);
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_VAE_LOSS_NAN,
                             "Invalid loss value (NaN/Inf)");
        /* Do NOT update EMA — NaN/Inf would corrupt the running average permanently */
        nimcp_mutex_unlock(ctx->mutex);
        return NAN;
    }

    /* Update EMA statistics — only reached when loss is finite */
    if (ctx->loss_computations == 0) {
        ctx->ema_recon = recon_loss;
        ctx->ema_kl = kl_raw;
    } else {
        ctx->ema_recon = ctx->ema_alpha * ctx->ema_recon + (1.0f - ctx->ema_alpha) * recon_loss;
        ctx->ema_kl = ctx->ema_alpha * ctx->ema_kl + (1.0f - ctx->ema_alpha) * kl_raw;
    }

    ctx->loss_computations++;

    /* Fill breakdown if provided */
    if (breakdown) {
        breakdown->total_loss = total_loss;
        breakdown->elbo = -total_loss;
        breakdown->free_energy = total_loss;
        breakdown->recon_loss = recon_loss;
        breakdown->kl_loss = kl_weighted;
        breakdown->kl_raw = kl_raw;
        breakdown->current_beta = ctx->current_kl_weight;
        breakdown->current_step = ctx->step;

        uint32_t batch_size = (x->shape.rank >= 1) ? x->shape.dims[0] : 1;
        breakdown->batch_size = batch_size;
        breakdown->recon_per_sample = recon_loss * batch_size / (float)nimcp_tensor_numel(x);
        breakdown->kl_per_sample = kl_raw;

        /* Count active units */
        if (kl_per_dim && breakdown->latent_dim > 0) {
            breakdown->active_units = vae_loss_count_active_units(
                kl_per_dim, breakdown->latent_dim, VAE_LOSS_ACTIVE_THRESHOLD);
            breakdown->active_ratio = (float)breakdown->active_units / (float)breakdown->latent_dim;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);

    return total_loss;
}

float vae_loss_compute_stateless(const vae_loss_config_t* config,
                                 const nimcp_tensor_t* x,
                                 const nimcp_tensor_t* x_recon,
                                 const nimcp_tensor_t* mu,
                                 const nimcp_tensor_t* log_var)
{
    if (!config || !x || !x_recon || !mu || !log_var) {
        return NAN;
    }

    /* Compute reconstruction loss */
    float recon_loss = 0.0f;

    switch (config->recon_type) {
        case VAE_RECON_MSE:
            recon_loss = vae_loss_mse(x, x_recon, config->aggregation);
            break;
        case VAE_RECON_BCE:
            recon_loss = vae_loss_bce(x, x_recon, config->aggregation);
            break;
        default:
            recon_loss = vae_loss_mse(x, x_recon, config->aggregation);
    }

    recon_loss *= config->recon_weight;

    /* Compute KL divergence */
    float kl = vae_loss_kl_standard_normal(mu, log_var, config->kl_reduction,
                                           config->aggregation);

    /* Apply β weighting */
    float kl_weighted = config->beta * kl;

    return recon_loss + kl_weighted;
}

/* ============================================================================
 * FEP Integration
 * ============================================================================ */

float vae_loss_free_energy(vae_loss_ctx_t* ctx,
                           const nimcp_tensor_t* x,
                           const nimcp_tensor_t* x_recon,
                           const nimcp_tensor_t* mu,
                           const nimcp_tensor_t* log_var,
                           float* prediction_error,
                           float* complexity_cost)
{
    if (!ctx || !ctx->is_initialized) {
        return NAN;
    }

    /* Compute components */
    float recon = 0.0f;

    switch (ctx->config.recon_type) {
        case VAE_RECON_MSE:
            recon = vae_loss_mse(x, x_recon, ctx->config.aggregation);
            break;
        case VAE_RECON_BCE:
            recon = vae_loss_bce(x, x_recon, ctx->config.aggregation);
            break;
        case VAE_RECON_GAUSSIAN_NLL:
            recon = vae_loss_gaussian_nll(x, x_recon, NULL, ctx->config.aggregation);
            break;
        default:
            recon = vae_loss_mse(x, x_recon, ctx->config.aggregation);
    }

    float kl = vae_loss_kl_standard_normal(mu, log_var, ctx->config.kl_reduction,
                                           ctx->config.aggregation);

    /* FEP decomposition:
     * Free Energy = Prediction Error + Complexity Cost
     * Prediction Error = -E[log p(x|z)] ≈ Reconstruction Loss
     * Complexity Cost = KL[q(z|x) || p(z)]
     */

    if (prediction_error) {
        *prediction_error = recon;
    }

    if (complexity_cost) {
        *complexity_cost = kl;
    }

    return recon + kl;
}

float vae_loss_precision_weighted(const nimcp_tensor_t* x,
                                  const nimcp_tensor_t* x_recon,
                                  const nimcp_tensor_t* precision,
                                  vae_loss_aggregation_t reduction)
{
    if (!x || !x_recon || !precision || !x->data || !x_recon->data || !precision->data) {
        return NAN;
    }

    uint32_t total = nimcp_tensor_numel(x);
    if (total == 0) return NAN;

    const float* x_data = (const float*)x->data;
    const float* recon_data = (const float*)x_recon->data;
    const float* prec_data = (const float*)precision->data;

    uint32_t prec_size = nimcp_tensor_numel(precision);
    bool scalar_prec = (prec_size == 1);

    float sum = 0.0f;

    for (uint32_t i = 0; i < total; i++) {
        float diff = x_data[i] - recon_data[i];
        float p = scalar_prec ? prec_data[0] : prec_data[i % prec_size];
        sum += p * diff * diff;
    }

    uint32_t batch_size = (x->shape.rank >= 1) ? x->shape.dims[0] : 1;

    if (reduction == VAE_LOSS_AGG_MEAN) {
        return sum / (float)total;
    }
    return sum / (float)batch_size;
}

void vae_loss_to_fep(const vae_loss_breakdown_t* breakdown,
                     float* fep_free_energy,
                     float* fep_accuracy,
                     float* fep_complexity)
{
    if (!breakdown) return;

    if (fep_free_energy) {
        *fep_free_energy = breakdown->free_energy;
    }

    if (fep_accuracy) {
        /* Accuracy term is negative reconstruction loss */
        *fep_accuracy = -breakdown->recon_loss;
    }

    if (fep_complexity) {
        *fep_complexity = breakdown->kl_raw;
    }
}

/* ============================================================================
 * Annealing Implementation
 * ============================================================================ */

float vae_loss_anneal_step(vae_loss_ctx_t* ctx)
{
    if (!ctx || !ctx->is_initialized) {
        return 1.0f;
    }

    nimcp_mutex_lock(ctx->mutex);

    ctx->step++;

    if (ctx->config.use_cyclical_annealing) {
        ctx->current_kl_weight = vae_loss_cyclical_anneal(
            ctx->step,
            ctx->config.cycle_steps,
            ctx->config.num_cycles,
            ctx->config.kl_anneal_steps * ctx->config.num_cycles);
    } else if (ctx->config.use_kl_annealing) {
        ctx->current_kl_weight = vae_loss_linear_anneal(
            ctx->step,
            ctx->config.kl_anneal_start,
            ctx->config.kl_anneal_end,
            ctx->config.kl_anneal_steps);
    }

    float weight = ctx->current_kl_weight;
    nimcp_mutex_unlock(ctx->mutex);

    return weight;
}

float vae_loss_get_kl_weight(const vae_loss_ctx_t* ctx)
{
    if (!ctx) return 1.0f;
    return ctx->current_kl_weight;
}

int vae_loss_set_step(vae_loss_ctx_t* ctx, uint32_t step)
{
    if (!ctx || !ctx->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_loss_set_step: required parameter is NULL (ctx, ctx->is_initialized)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->step = step;

    /* Recompute KL weight for this step */
    if (ctx->config.use_cyclical_annealing) {
        ctx->current_kl_weight = vae_loss_cyclical_anneal(
            step, ctx->config.cycle_steps, ctx->config.num_cycles,
            ctx->config.kl_anneal_steps * ctx->config.num_cycles);
    } else if (ctx->config.use_kl_annealing) {
        ctx->current_kl_weight = vae_loss_linear_anneal(
            step, ctx->config.kl_anneal_start, ctx->config.kl_anneal_end,
            ctx->config.kl_anneal_steps);
    }

    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

float vae_loss_linear_anneal(uint32_t step,
                             float start_value,
                             float end_value,
                             uint32_t total_steps)
{
    if (total_steps == 0) return end_value;
    if (step >= total_steps) return end_value;

    float t = (float)step / (float)total_steps;
    return start_value + t * (end_value - start_value);
}

float vae_loss_cyclical_anneal(uint32_t step,
                               uint32_t cycle_steps,
                               uint32_t num_cycles,
                               uint32_t total_steps)
{
    if (cycle_steps == 0 || num_cycles == 0) return 1.0f;

    uint32_t cycle = step / cycle_steps;
    if (cycle >= num_cycles) return 1.0f;

    uint32_t step_in_cycle = step % cycle_steps;
    float t = (float)step_in_cycle / (float)cycle_steps;

    /* Linear ramp from 0 to 1 within each cycle */
    return t;
}

/* ============================================================================
 * Gradient Computation
 * ============================================================================ */

int vae_loss_recon_gradient(const nimcp_tensor_t* x,
                            const nimcp_tensor_t* x_recon,
                            vae_recon_loss_type_t loss_type,
                            nimcp_tensor_t* d_recon)
{
    if (!x || !x_recon || !d_recon || !x->data || !x_recon->data || !d_recon->data) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_INVALID_PARAM,
                             "NULL tensor in recon gradient");
        return -1;
    }

    uint32_t total = nimcp_tensor_numel(x);
    if (total == 0 || total != nimcp_tensor_numel(x_recon) || total != nimcp_tensor_numel(d_recon)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_loss_recon_gradient: total is zero");
        return -1;
    }

    const float* x_data = (const float*)x->data;
    const float* recon_data = (const float*)x_recon->data;
    float* grad_data = (float*)d_recon->data;

    uint32_t batch_size = (x->shape.rank >= 1) ? x->shape.dims[0] : 1;
    float scale = 2.0f / (float)total;

    switch (loss_type) {
        case VAE_RECON_MSE:
            /* d(MSE)/d(recon) = 2 * (recon - x) / N */
            for (uint32_t i = 0; i < total; i++) {
                grad_data[i] = scale * (recon_data[i] - x_data[i]);
            }
            break;

        case VAE_RECON_BCE:
            /* d(BCE)/d(recon) = -(x/recon - (1-x)/(1-recon)) / N */
            for (uint32_t i = 0; i < total; i++) {
                float target = nimcp_clampf(x_data[i], 0.0f, 1.0f);
                float pred = nimcp_clampf(recon_data[i], VAE_LOSS_LOG_EPSILON, 1.0f - VAE_LOSS_LOG_EPSILON);
                grad_data[i] = (-(target / pred) + (1.0f - target) / (1.0f - pred)) / (float)total;
            }
            break;

        case VAE_RECON_HUBER:
            /* Huber gradient */
            for (uint32_t i = 0; i < total; i++) {
                float diff = recon_data[i] - x_data[i];
                float abs_diff = fabsf(diff);
                if (abs_diff <= 1.0f) {
                    grad_data[i] = diff / (float)batch_size;
                } else {
                    grad_data[i] = (diff > 0 ? 1.0f : -1.0f) / (float)batch_size;
                }
            }
            break;

        default:
            /* Default to MSE gradient */
            for (uint32_t i = 0; i < total; i++) {
                grad_data[i] = scale * (recon_data[i] - x_data[i]);
            }
    }

    return 0;
}

int vae_loss_kl_gradient(const nimcp_tensor_t* mu,
                         const nimcp_tensor_t* log_var,
                         float beta,
                         nimcp_tensor_t* d_mu,
                         nimcp_tensor_t* d_log_var)
{
    if (!mu || !log_var || !mu->data || !log_var->data) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_INVALID_PARAM,
                             "NULL tensor in KL gradient");
        return -1;
    }

    uint32_t total = nimcp_tensor_numel(mu);
    if (total == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_loss_kl_gradient: total is zero");
        return -1;
    }

    const float* mu_data = (const float*)mu->data;
    const float* log_var_data = (const float*)log_var->data;

    uint32_t batch_size = (mu->shape.rank >= 1) ? mu->shape.dims[0] : 1;
    float scale = beta / (float)batch_size;

    /* d(KL)/d(mu) = mu * beta / batch_size */
    if (d_mu && d_mu->data) {
        float* d_mu_data = (float*)d_mu->data;
        for (uint32_t i = 0; i < total; i++) {
            d_mu_data[i] = scale * mu_data[i];
        }
    }

    /* d(KL)/d(log_var) = 0.5 * (exp(log_var) - 1) * beta / batch_size */
    if (d_log_var && d_log_var->data) {
        float* d_lv_data = (float*)d_log_var->data;
        for (uint32_t i = 0; i < total; i++) {
            float lv = nimcp_clampf(log_var_data[i], -10.0f, 10.0f);
            d_lv_data[i] = 0.5f * scale * (expf(lv) - 1.0f);
        }
    }

    return 0;
}

/* ============================================================================
 * Monitoring Implementation
 * ============================================================================ */

vae_loss_breakdown_t* vae_loss_breakdown_create(uint32_t latent_dim)
{
    vae_loss_breakdown_t* breakdown = nimcp_calloc(1, sizeof(vae_loss_breakdown_t));
    if (!breakdown) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_LOSS_MODULE_ID, NIMCP_ERROR_VAE_NO_MEMORY,
                             "Failed to allocate loss breakdown");
        return NULL;
    }

    if (latent_dim > 0) {
        breakdown->kl_per_dim = nimcp_calloc(latent_dim, sizeof(float));
        if (!breakdown->kl_per_dim) {
            nimcp_free(breakdown);
            breakdown = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_loss_breakdown_create: breakdown->kl_per_dim is NULL");
            return NULL;
        }
        breakdown->latent_dim = latent_dim;
    }

    return breakdown;
}

void vae_loss_breakdown_free(vae_loss_breakdown_t* breakdown)
{
    if (!breakdown) return;

    if (breakdown->kl_per_dim) {
        nimcp_free(breakdown->kl_per_dim);
    }

    nimcp_free(breakdown);
    breakdown = NULL;
}

bool vae_loss_is_invalid(float loss)
{
    return isnan(loss) || isinf(loss);
}

void vae_loss_get_stats(const vae_loss_ctx_t* ctx,
                        uint32_t* nan_count,
                        uint32_t* inf_count,
                        uint64_t* total_computations)
{
    if (!ctx) return;

    if (nan_count) *nan_count = ctx->nan_count;
    if (inf_count) *inf_count = ctx->inf_count;
    if (total_computations) *total_computations = ctx->loss_computations;
}

uint32_t vae_loss_count_active_units(const float* kl_per_dim,
                                     uint32_t latent_dim,
                                     float threshold)
{
    if (!kl_per_dim || latent_dim == 0) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < latent_dim; i++) {
        if (kl_per_dim[i] > threshold) {
            count++;
        }
    }

    return count;
}

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

void vae_loss_set_health_agent(vae_loss_ctx_t* ctx,
                               nimcp_health_agent_t* agent)
{
    if (!ctx) return;

    /* Health agent integration placeholder */
    /* In full implementation, would register heartbeat callback */

    NIMCP_LOG_DEBUG("VAE Loss: Health agent set");
}
