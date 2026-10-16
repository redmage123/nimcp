/**
 * @file nimcp_quantization_aware.c
 * @brief Implementation of Quantization-Aware Training (QAT) for NIMCP
 *
 * WHAT: Train models with simulated quantization for efficient deployment
 * WHY:  Enable INT8/INT4 inference with minimal accuracy loss
 * HOW:  Fake quantization during training, learn quantization-robust weights
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "training/nimcp_quantization_aware.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdbool.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(quantization_aware)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Range observer for a tensor
 */
typedef struct {
    char name[NIMCP_LABEL_BUFFER_SIZE];                  /**< Tensor name */
    qat_target_t target;             /**< Weight or activation */
    bool active;                     /**< Whether observer is active */

    /* Range statistics */
    float min_val;                   /**< Observed minimum */
    float max_val;                   /**< Observed maximum */
    float ema_min;                   /**< EMA minimum */
    float ema_max;                   /**< EMA maximum */
    uint64_t observation_count;      /**< Number of observations */

    /* Histogram (if using histogram observer) */
    uint32_t* histogram;             /**< Value histogram */
    uint32_t num_bins;               /**< Number of histogram bins */
    float hist_min;                  /**< Histogram range min */
    float hist_max;                  /**< Histogram range max */

    /* Computed quantization parameters */
    qat_params_t params;             /**< Quantization parameters */
    bool params_valid;               /**< Whether params are computed */

    /* Learnable parameters (LSQ) */
    float learned_scale;             /**< Learned scale parameter */
    int32_t learned_zero_point;      /**< Learned zero point */
    float scale_grad;                /**< Accumulated scale gradient */
} observer_t;

/**
 * @brief QAT context
 */
struct qat_ctx_s {
    qat_config_t config;             /**< Configuration */

    /* Observers */
    observer_t observers[QAT_MAX_OBSERVERS]; /**< Registered observers */
    uint32_t num_observers;          /**< Number of observers */
    bool observers_frozen;           /**< Observers frozen flag */

    /* Calibration state */
    bool calibration_complete;       /**< Calibration done */
    uint32_t calibration_batches;    /**< Calibration batches so far */

    /* Integration handles */
    void* brain_factory;             /**< Brain factory */

    /* Statistics */
    qat_stats_t stats;               /**< Runtime statistics */

    /* Thread safety */
    nimcp_mutex_t* mutex;            /**< Mutex for thread safety */
};

//=============================================================================
// Name Strings
//=============================================================================

static const char* dtype_names[] = {
    "INT8", "UINT8", "INT4", "INT2", "INT1",
    "FP8_E4M3", "FP8_E5M2", "FP4", "Ternary"
};

static const uint32_t dtype_bits[] = {
    8, 8, 4, 2, 1, 8, 8, 4, 2
};

static const char* scheme_names[] = {
    "Symmetric", "Affine", "Power-of-2"
};

//=============================================================================
// Forward Declarations
//=============================================================================

static void compute_scale_zp(float min_val, float max_val, qat_dtype_t dtype,
                             qat_scheme_t scheme, float* scale, int32_t* zero_point);
static void get_qmin_qmax(qat_dtype_t dtype, int32_t* qmin, int32_t* qmax);
int qat_default_config(qat_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_default_config: config is NULL");
        return -1;
    }
    memset(config, 0, sizeof(qat_config_t));

    /* Default dtypes */
    config->default_weight_dtype = QAT_DTYPE_INT8;
    config->default_activation_dtype = QAT_DTYPE_INT8;
    config->default_scheme = QAT_SCHEME_SYMMETRIC;
    config->default_granularity = QAT_GRANULARITY_TENSOR;

    /* Observer: MinMax with EMA */
    config->observer.method = QAT_OBSERVER_MINMAX;
    config->observer.ema_decay = QAT_DEFAULT_EMA_DECAY;
    config->observer.percentile = 0.999f;
    config->observer.num_bins = 2048;
    config->observer.calibration_batches = QAT_DEFAULT_CALIBRATION_BATCHES;
    config->observer.symmetric = true;

    /* Fake quantization: STE */
    config->fake_quant.method = QAT_FAKE_QUANT_STE;
    config->fake_quant.learn_scale = false;
    config->fake_quant.learn_zero_point = false;
    config->fake_quant.initial_scale = 1.0f;
    config->fake_quant.scale_lr_multiplier = 1.0f;
    config->fake_quant.grad_scale_factor = 1.0f;

    /* Training schedule */
    config->warmup_epochs = 0;
    config->freeze_bn_epochs = 0;
    config->start_with_calibration = false;

    /* No per-layer configs by default */
    config->layer_configs = NULL;
    config->num_layer_configs = 0;

    /* Integration */
    config->integrate_tensor_layer = true;
    config->integrate_brain_factory = false;

    /* Debugging */
    config->verbose = false;
    config->track_statistics = true;

    return 0;
}

int qat_int4_config(qat_config_t* config) {
    int result = qat_default_config(config);
    if (result != 0) {
        return result;
    }

    /* INT4 settings */
    config->default_weight_dtype = QAT_DTYPE_INT4;
    config->default_activation_dtype = QAT_DTYPE_INT8;  /* Keep activations INT8 */

    /* Per-channel quantization recommended for INT4 */
    config->default_granularity = QAT_GRANULARITY_CHANNEL;

    /* LSQ for better INT4 accuracy */
    config->fake_quant.method = QAT_FAKE_QUANT_LSQ;
    config->fake_quant.learn_scale = true;

    return 0;
}

int qat_binary_config(qat_config_t* config) {
    int result = qat_default_config(config);
    if (result != 0) {
        return result;
    }

    /* Binary settings */
    config->default_weight_dtype = QAT_DTYPE_INT1;
    config->default_activation_dtype = QAT_DTYPE_INT1;
    config->default_scheme = QAT_SCHEME_SYMMETRIC;

    /* STE is standard for binary networks */
    config->fake_quant.method = QAT_FAKE_QUANT_STE;

    return 0;
}

qat_ctx_t* qat_create(const qat_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "qat_create: config is NULL");
        return NULL;
    }

    /* Validate configuration */
    if (qat_validate_config(config) != 0) {
        NIMCP_THROW(NIMCP_ERROR_CONFIG_INVALID, "qat_create: config validation failed");
        return NULL;
    }

    qat_ctx_t* ctx = nimcp_calloc(1, sizeof(qat_ctx_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(qat_config_t));

    /* Deep copy layer_configs if present (shallow memcpy only copies pointer) */
    if (config->layer_configs && config->num_layer_configs > 0) {
        size_t lc_size = config->num_layer_configs * sizeof(config->layer_configs[0]);
        ctx->config.layer_configs = nimcp_malloc(lc_size);
        if (!ctx->config.layer_configs) {
            nimcp_free(ctx);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qat_create: layer_configs deep copy failed");
            return NULL;
        }
        memcpy(ctx->config.layer_configs, config->layer_configs, lc_size);
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    ctx->mutex = nimcp_mutex_create(&attr);
    if (!ctx->mutex) {
        if (ctx->config.layer_configs) {
            nimcp_free(ctx->config.layer_configs);
        }
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qat_create: ctx->mutex is NULL");
        return NULL;
    }

    /* Initialize observers */
    ctx->num_observers = 0;
    ctx->observers_frozen = false;
    ctx->calibration_complete = false;
    ctx->calibration_batches = 0;

    /* Reset statistics */
    memset(&ctx->stats, 0, sizeof(qat_stats_t));

    if (config->verbose) {
        printf("[QAT] Created context: weight=%s, activation=%s, scheme=%s\n",
               qat_dtype_name(config->default_weight_dtype),
               qat_dtype_name(config->default_activation_dtype),
               qat_scheme_name(config->default_scheme));
    }

    return ctx;
}

void qat_destroy(qat_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Clean up observers */
    for (uint32_t i = 0; i < ctx->num_observers; i++) {
        observer_t* obs = &ctx->observers[i];
        if (obs->histogram) {
            nimcp_free(obs->histogram);
        }
        if (obs->params.scales) {
            nimcp_free(obs->params.scales);
        }
        if (obs->params.zero_points) {
            nimcp_free(obs->params.zero_points);
        }
    }

    /* Free deep-copied layer configs */
    if (ctx->config.layer_configs) {
        nimcp_free(ctx->config.layer_configs);
    }

    /* Destroy mutex */
    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
    }

    nimcp_free(ctx);
}

//=============================================================================
// Observer API Implementation
//=============================================================================

int qat_register_observer(
    qat_ctx_t* ctx,
    const char* name,
    qat_target_t target
) {
    if (!ctx || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_register_observer: required parameter is NULL (ctx, name)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->num_observers >= QAT_MAX_OBSERVERS) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "qat_register_observer: capacity exceeded");
        return -1;
    }

    int id = (int)ctx->num_observers;
    observer_t* obs = &ctx->observers[id];

    memset(obs, 0, sizeof(observer_t));
    strncpy(obs->name, name, sizeof(obs->name) - 1);
    obs->target = target;
    obs->active = true;
    obs->min_val = FLT_MAX;
    obs->max_val = -FLT_MAX;
    obs->ema_min = FLT_MAX;
    obs->ema_max = -FLT_MAX;
    obs->observation_count = 0;

    /* Set default params */
    obs->params.dtype = (target == QAT_TARGET_WEIGHTS) ?
        ctx->config.default_weight_dtype : ctx->config.default_activation_dtype;
    obs->params.scheme = ctx->config.default_scheme;
    obs->params.granularity = ctx->config.default_granularity;
    obs->params_valid = false;

    /* Initialize histogram if using histogram observer */
    if (ctx->config.observer.method == QAT_OBSERVER_HISTOGRAM) {
        obs->num_bins = ctx->config.observer.num_bins;
        obs->histogram = nimcp_calloc(obs->num_bins, sizeof(uint32_t));
        if (!obs->histogram) {
            memset(obs, 0, sizeof(observer_t));  /* Clean up partially-initialized slot */
            nimcp_mutex_unlock(ctx->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                                  "qat_register_observer: histogram alloc failed");
            return -1;
        }
    }

    /* Initialize LSQ parameters */
    obs->learned_scale = ctx->config.fake_quant.initial_scale;
    obs->learned_zero_point = 0;

    ctx->num_observers++;

    if (ctx->config.verbose) {
        printf("[QAT] Registered observer '%s' for %s (id=%d)\n",
               name, (target == QAT_TARGET_WEIGHTS) ? "weights" : "activations", id);
    }

    nimcp_mutex_unlock(ctx->mutex);
    return id;
}

int qat_observe(
    qat_ctx_t* ctx,
    int observer_id,
    const nimcp_tensor_t* tensor
) {
    if (!ctx || !tensor || observer_id < 0 || observer_id >= (int)ctx->num_observers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_observe: required parameter is NULL (ctx, tensor)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    observer_t* obs = &ctx->observers[observer_id];
    if (!obs->active || ctx->observers_frozen) {
        nimcp_mutex_unlock(ctx->mutex);
        return 0;
    }

    size_t count = nimcp_tensor_numel(tensor);
    const float* data = nimcp_tensor_data((nimcp_tensor_t*)tensor);

    if (!data || count == 0) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qat_observe: data is NULL");
        return -1;
    }

    /* Update range statistics */
    float batch_min = FLT_MAX, batch_max = -FLT_MAX;
    for (size_t i = 0; i < count; i++) {
        if (data[i] < batch_min) batch_min = data[i];
        if (data[i] > batch_max) batch_max = data[i];
    }

    switch (ctx->config.observer.method) {
        case QAT_OBSERVER_MINMAX:
            /* Track global min/max */
            if (batch_min < obs->min_val) obs->min_val = batch_min;
            if (batch_max > obs->max_val) obs->max_val = batch_max;
            break;

        case QAT_OBSERVER_MOVING_AVG:
            /* Exponential moving average */
            if (obs->observation_count == 0) {
                obs->ema_min = batch_min;
                obs->ema_max = batch_max;
            } else {
                float decay = ctx->config.observer.ema_decay;
                obs->ema_min = decay * obs->ema_min + (1.0f - decay) * batch_min;
                NIMCP_EMA_GUARD(obs->ema_min, batch_min);
                obs->ema_max = decay * obs->ema_max + (1.0f - decay) * batch_max;
                NIMCP_EMA_GUARD(obs->ema_max, batch_max);
            }
            obs->min_val = obs->ema_min;
            obs->max_val = obs->ema_max;
            break;

        case QAT_OBSERVER_HISTOGRAM:
            /* Update histogram */
            if (obs->histogram) {
                /* Expand histogram range if needed */
                if (obs->observation_count == 0) {
                    obs->hist_min = batch_min;
                    obs->hist_max = batch_max;
                } else {
                    if (batch_min < obs->hist_min) obs->hist_min = batch_min;
                    if (batch_max > obs->hist_max) obs->hist_max = batch_max;
                }

                float range = obs->hist_max - obs->hist_min;
                if (range > 1e-8f) {
                    for (size_t i = 0; i < count; i++) {
                        float normalized = (data[i] - obs->hist_min) / range;
                        uint32_t bin = (uint32_t)(normalized * (obs->num_bins - 1));
                        if (bin >= obs->num_bins) bin = obs->num_bins - 1;
                        obs->histogram[bin]++;
                    }
                }

                /* Compute percentile bounds */
                uint64_t total = 0;
                for (uint32_t i = 0; i < obs->num_bins; i++) {
                    total += obs->histogram[i];
                }
                if (total == 0) break;

                float percentile = ctx->config.observer.percentile;
                uint64_t lower_target = (uint64_t)(total * (1.0f - percentile));
                uint64_t upper_target = (uint64_t)(total * percentile);

                uint64_t cumsum = 0;
                uint32_t lower_bin = 0, upper_bin = obs->num_bins - 1;
                bool lower_found = false;
                for (uint32_t i = 0; i < obs->num_bins; i++) {
                    cumsum += obs->histogram[i];
                    if (cumsum >= lower_target && !lower_found) {
                        lower_bin = i;
                        lower_found = true;
                    }
                    if (cumsum >= upper_target) {
                        upper_bin = i;
                        break;
                    }
                }

                obs->min_val = obs->hist_min + (float)lower_bin / obs->num_bins * range;
                obs->max_val = obs->hist_min + (float)upper_bin / obs->num_bins * range;
            }
            break;

        default:
            if (batch_min < obs->min_val) obs->min_val = batch_min;
            if (batch_max > obs->max_val) obs->max_val = batch_max;
            break;
    }

    obs->observation_count++;

    /* Update observed range in params */
    obs->params.observed_min = obs->min_val;
    obs->params.observed_max = obs->max_val;

    /* Compute quantization parameters */
    compute_scale_zp(obs->min_val, obs->max_val, obs->params.dtype,
                     obs->params.scheme, &obs->params.scale, &obs->params.zero_point);
    obs->params_valid = true;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int qat_get_params(
    qat_ctx_t* ctx,
    int observer_id,
    qat_params_t* params
) {
    if (!ctx || !params || observer_id < 0 || observer_id >= (int)ctx->num_observers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_get_params: required parameter is NULL (ctx, params)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    observer_t* obs = &ctx->observers[observer_id];
    if (!obs->params_valid) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_get_params: obs->params_valid is NULL");
        return -1;
    }

    memcpy(params, &obs->params, sizeof(qat_params_t));

    /* Override with learned parameters if using LSQ */
    if (ctx->config.fake_quant.learn_scale) {
        params->scale = obs->learned_scale;
    }
    if (ctx->config.fake_quant.learn_zero_point) {
        params->zero_point = obs->learned_zero_point;
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int qat_calibrate(qat_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Mark calibration as complete */
    ctx->calibration_complete = true;

    /* Compute final quantization parameters for all observers */
    for (uint32_t i = 0; i < ctx->num_observers; i++) {
        observer_t* obs = &ctx->observers[i];
        if (obs->active && obs->observation_count > 0) {
            compute_scale_zp(obs->min_val, obs->max_val, obs->params.dtype,
                            obs->params.scheme, &obs->params.scale, &obs->params.zero_point);
            obs->params_valid = true;

            if (ctx->config.verbose) {
                printf("[QAT] Calibrated '%s': range=[%.4f, %.4f], scale=%.6f, zp=%d\n",
                       obs->name, obs->min_val, obs->max_val,
                       obs->params.scale, obs->params.zero_point);
            }
        }
    }

    if (ctx->config.verbose) {
        printf("[QAT] Calibration complete for %u observers\n", ctx->num_observers);
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int qat_freeze_observers(qat_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->observers_frozen = true;

    if (ctx->config.verbose) {
        printf("[QAT] Observers frozen\n");
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Fake Quantization API Implementation
//=============================================================================

int qat_fake_quantize(
    qat_ctx_t* ctx,
    nimcp_tensor_t* tensor,
    const qat_params_t* params
) {
    if (!ctx || !tensor || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_fake_quantize: required parameter is NULL (ctx, tensor, params)");
        return -1;
    }

    size_t count = nimcp_tensor_numel(tensor);
    float* data = nimcp_tensor_data(tensor);

    if (!data || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qat_fake_quantize: data is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    float scale = params->scale;
    int32_t zero_point = params->zero_point;

    /* Get quantization range */
    int32_t qmin, qmax;
    get_qmin_qmax(params->dtype, &qmin, &qmax);

    float inv_scale = (scale > 1e-10f) ? 1.0f / scale : 1.0f;

    /* Fake quantize: round(x/scale + zp) then dequantize */
    for (size_t i = 0; i < count; i++) {
        /* Quantize */
        float scaled = data[i] * inv_scale + (float)zero_point;
        int32_t quantized = (int32_t)roundf(scaled);
        quantized = (quantized < qmin) ? qmin : ((quantized > qmax) ? qmax : quantized);

        /* Dequantize */
        data[i] = ((float)quantized - (float)zero_point) * scale;
    }

    /* Update statistics */
    ctx->stats.total_steps++;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int qat_fake_quantize_learned(
    qat_ctx_t* ctx,
    nimcp_tensor_t* tensor,
    int observer_id
) {
    if (!ctx || !tensor || observer_id < 0 || observer_id >= (int)ctx->num_observers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_fake_quantize_learned: required parameter is NULL (ctx, tensor)");
        return -1;
    }

    observer_t* obs = &ctx->observers[observer_id];
    if (!obs->params_valid) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_fake_quantize_learned: obs->params_valid is NULL");
        return -1;
    }

    qat_params_t params;
    memcpy(&params, &obs->params, sizeof(qat_params_t));

    /* Use learned scale */
    if (ctx->config.fake_quant.learn_scale) {
        params.scale = obs->learned_scale;
    }
    if (ctx->config.fake_quant.learn_zero_point) {
        params.zero_point = obs->learned_zero_point;
    }

    return qat_fake_quantize(ctx, tensor, &params);
}

int qat_fake_quantize_backward(
    qat_ctx_t* ctx,
    const nimcp_tensor_t* grad_output,
    const nimcp_tensor_t* tensor,
    const qat_params_t* params,
    nimcp_tensor_t* grad_input
) {
    if (!ctx || !grad_output || !tensor || !params || !grad_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_fake_quantize_backward: required parameter is NULL (ctx, grad_output, tensor, params, grad_input)");
        return -1;
    }

    size_t count = nimcp_tensor_numel(grad_output);
    size_t tensor_count = nimcp_tensor_numel(tensor);
    size_t grad_input_count = nimcp_tensor_numel(grad_input);

    if (count != tensor_count || count != grad_input_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qat_fake_quantize_backward: validation failed");
        return -1;
    }

    const float* grad_out_data = nimcp_tensor_data((nimcp_tensor_t*)grad_output);
    const float* tensor_data = nimcp_tensor_data((nimcp_tensor_t*)tensor);
    float* grad_in_data = nimcp_tensor_data(grad_input);

    if (!grad_out_data || !tensor_data || !grad_in_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_fake_quantize_backward: required parameter is NULL (grad_out_data, tensor_data, grad_in_data)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    float scale = params->scale;
    int32_t zero_point = params->zero_point;

    int32_t qmin, qmax;
    get_qmin_qmax(params->dtype, &qmin, &qmax);

    float lower_bound = ((float)qmin - (float)zero_point) * scale;
    float upper_bound = ((float)qmax - (float)zero_point) * scale;

    switch (ctx->config.fake_quant.method) {
        case QAT_FAKE_QUANT_STE:
            /* Straight-Through Estimator: pass gradient where not clipped */
            for (size_t i = 0; i < count; i++) {
                if (tensor_data[i] >= lower_bound && tensor_data[i] <= upper_bound) {
                    grad_in_data[i] = grad_out_data[i] * ctx->config.fake_quant.grad_scale_factor;
                } else {
                    grad_in_data[i] = 0.0f;  /* Zero gradient for clipped values */
                }
            }
            break;

        case QAT_FAKE_QUANT_LSQ:
            /* LSQ: scaled gradient for quantization levels */
            {
                float grad_scale = 1.0f / sqrtf((float)count * (float)qmax);
                for (size_t i = 0; i < count; i++) {
                    if (tensor_data[i] < lower_bound) {
                        grad_in_data[i] = grad_out_data[i] * grad_scale;
                    } else if (tensor_data[i] > upper_bound) {
                        grad_in_data[i] = grad_out_data[i] * grad_scale;
                    } else {
                        grad_in_data[i] = grad_out_data[i];
                    }
                }
            }
            break;

        case QAT_FAKE_QUANT_PACT:
            /* PACT: parameterized clipping activation */
            for (size_t i = 0; i < count; i++) {
                if (tensor_data[i] >= 0.0f && tensor_data[i] <= upper_bound) {
                    grad_in_data[i] = grad_out_data[i];
                } else if (tensor_data[i] > upper_bound) {
                    grad_in_data[i] = 0.0f;  /* Gradient goes to alpha param */
                } else {
                    grad_in_data[i] = 0.0f;
                }
            }
            break;

        default:
            /* Default to STE */
            memcpy(grad_in_data, grad_out_data, count * sizeof(float));
            break;
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Quantization Operations Implementation
//=============================================================================

int qat_quantize(
    qat_ctx_t* ctx,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    const qat_params_t* params
) {
    if (!ctx || !input || !output || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_quantize: required parameter is NULL (ctx, input, output, params)");
        return -1;
    }

    size_t count = nimcp_tensor_numel(input);
    size_t output_count = nimcp_tensor_numel(output);

    if (count != output_count || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qat_quantize: count is zero");
        return -1;
    }

    const float* input_data = nimcp_tensor_data((nimcp_tensor_t*)input);
    float* output_data = nimcp_tensor_data(output);

    if (!input_data || !output_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_quantize: required parameter is NULL (input_data, output_data)");
        return -1;
    }

    float scale = params->scale;
    int32_t zero_point = params->zero_point;

    int32_t qmin, qmax;
    get_qmin_qmax(params->dtype, &qmin, &qmax);

    float inv_scale = (scale > 1e-10f) ? 1.0f / scale : 1.0f;

    /* Quantize: q = nimcp_clampf(round(x/scale + zp), qmin, qmax) */
    for (size_t i = 0; i < count; i++) {
        float scaled = input_data[i] * inv_scale + (float)zero_point;
        int32_t quantized = (int32_t)roundf(scaled);
        quantized = (quantized < qmin) ? qmin : ((quantized > qmax) ? qmax : quantized);
        output_data[i] = (float)quantized;
    }

    return 0;
}

int qat_dequantize(
    qat_ctx_t* ctx,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    const qat_params_t* params
) {
    if (!ctx || !input || !output || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_dequantize: required parameter is NULL (ctx, input, output, params)");
        return -1;
    }

    size_t count = nimcp_tensor_numel(input);
    size_t output_count = nimcp_tensor_numel(output);

    if (count != output_count || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qat_dequantize: count is zero");
        return -1;
    }

    const float* input_data = nimcp_tensor_data((nimcp_tensor_t*)input);
    float* output_data = nimcp_tensor_data(output);

    if (!input_data || !output_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_dequantize: required parameter is NULL (input_data, output_data)");
        return -1;
    }

    float scale = params->scale;
    int32_t zero_point = params->zero_point;

    /* Dequantize: x = (q - zp) * scale */
    for (size_t i = 0; i < count; i++) {
        output_data[i] = (input_data[i] - (float)zero_point) * scale;
    }

    return 0;
}

int qat_matmul(
    qat_ctx_t* ctx,
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b,
    nimcp_tensor_t* c,
    const qat_params_t* a_params,
    const qat_params_t* b_params,
    const qat_params_t* c_params
) {
    if (!ctx || !a || !b || !c || !a_params || !b_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_matmul: required parameter is NULL (ctx, a, b, c, a_params, b_params)");
        return -1;
    }

    /* For now, perform fake-quantized matmul (FP32 compute with quantization)
     * In real implementation, would use INT8 GEMM kernels */

    /* Get dimensions (assuming 2D tensors for simplicity) */
    /* Would need proper tensor shape handling for full implementation */

    size_t a_count = nimcp_tensor_numel(a);
    size_t b_count = nimcp_tensor_numel(b);
    size_t c_count = nimcp_tensor_numel(c);

    if (a_count == 0 || b_count == 0 || c_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qat_matmul: a_count is zero");
        return -1;
    }

    /* Get tensor shapes for matrix dimensions.
     * Assumes 2D tensors: A is [M, K], B is [K, N], C is [M, N] */
    const nimcp_tensor_shape_t* a_shape = nimcp_tensor_shape(a);
    const nimcp_tensor_shape_t* b_shape = nimcp_tensor_shape(b);
    const nimcp_tensor_shape_t* c_shape = nimcp_tensor_shape(c);

    if (!a_shape || !b_shape || !c_shape) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_matmul: tensor shape is NULL");
        return -1;
    }

    uint32_t M, K_a, K_b, N;
    if (a_shape->rank >= 2) {
        M = a_shape->dims[0];
        K_a = a_shape->dims[1];
    } else {
        /* 1D: treat as [1, K] */
        M = 1;
        K_a = a_shape->dims[0];
    }

    if (b_shape->rank >= 2) {
        K_b = b_shape->dims[0];
        N = b_shape->dims[1];
    } else {
        K_b = b_shape->dims[0];
        N = 1;
    }

    if (K_a != K_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qat_matmul: inner dimensions mismatch (K_a != K_b)");
        return -1;
    }
    uint32_t K = K_a;

    /* Verify output dimensions */
    if (c_count < (size_t)M * N) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qat_matmul: output tensor too small");
        return -1;
    }

    const float* a_data = nimcp_tensor_data((nimcp_tensor_t*)a);
    const float* b_data = nimcp_tensor_data((nimcp_tensor_t*)b);
    float* c_data = nimcp_tensor_data(c);

    if (!a_data || !b_data || !c_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_matmul: tensor data is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Get quantization ranges for A and B */
    int32_t a_qmin, a_qmax, b_qmin, b_qmax;
    get_qmin_qmax(a_params->dtype, &a_qmin, &a_qmax);
    get_qmin_qmax(b_params->dtype, &b_qmin, &b_qmax);

    float a_scale = a_params->scale;
    float b_scale = b_params->scale;
    int32_t a_zp = a_params->zero_point;
    int32_t b_zp = b_params->zero_point;

    float a_inv_scale = (a_scale > 1e-10f) ? 1.0f / a_scale : 1.0f;
    float b_inv_scale = (b_scale > 1e-10f) ? 1.0f / b_scale : 1.0f;

    /* Product scale: S_c = S_a * S_b */
    float c_scale = a_scale * b_scale;

    /* Fake-quantized matmul:
     * 1. Quantize A: q_a = clamp(round(a / S_a + ZP_a), qmin, qmax)
     * 2. Quantize B: q_b = clamp(round(b / S_b + ZP_b), qmin, qmax)
     * 3. Integer matmul: acc = sum((q_a - ZP_a) * (q_b - ZP_b))
     * 4. Dequantize: c = acc * S_a * S_b
     *
     * This simulates integer arithmetic while maintaining float precision
     * for gradient flow during training (straight-through estimator). */
    for (uint32_t m = 0; m < M; m++) {
        for (uint32_t n = 0; n < N; n++) {
            int64_t acc = 0;  /* Integer accumulator */

            for (uint32_t k = 0; k < K; k++) {
                /* Fake-quantize A[m,k] */
                float a_val = a_data[m * K + k];
                float a_scaled = a_val * a_inv_scale + (float)a_zp;
                int32_t q_a = (int32_t)roundf(a_scaled);
                q_a = (q_a < a_qmin) ? a_qmin : ((q_a > a_qmax) ? a_qmax : q_a);

                /* Fake-quantize B[k,n] */
                float b_val = b_data[k * N + n];
                float b_scaled = b_val * b_inv_scale + (float)b_zp;
                int32_t q_b = (int32_t)roundf(b_scaled);
                q_b = (q_b < b_qmin) ? b_qmin : ((q_b > b_qmax) ? b_qmax : q_b);

                /* Integer MAC (shifted by zero points) */
                acc += (int64_t)(q_a - a_zp) * (int64_t)(q_b - b_zp);
            }

            /* Dequantize accumulator */
            c_data[m * N + n] = (float)acc * c_scale;
        }
    }

    /* Optionally requantize output if c_params provided */
    if (c_params && c_params->scale > 0.0f) {
        int32_t c_qmin, c_qmax;
        get_qmin_qmax(c_params->dtype, &c_qmin, &c_qmax);
        float c_out_scale = c_params->scale;
        int32_t c_out_zp = c_params->zero_point;
        float c_inv_scale = (c_out_scale > 1e-10f) ? 1.0f / c_out_scale : 1.0f;

        for (size_t i = 0; i < (size_t)M * N; i++) {
            float scaled = c_data[i] * c_inv_scale + (float)c_out_zp;
            int32_t quantized = (int32_t)roundf(scaled);
            quantized = (quantized < c_qmin) ? c_qmin : ((quantized > c_qmax) ? c_qmax : quantized);
            c_data[i] = ((float)quantized - (float)c_out_zp) * c_out_scale;
        }
    }

    /* Update statistics */
    ctx->stats.total_steps++;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Export API Implementation
//=============================================================================

int qat_export(
    qat_ctx_t* ctx,
    nimcp_tensor_t** weights,
    uint32_t num_weights,
    qat_params_t** params,
    const char* filepath
) {
    if (!ctx || !weights || !params || !filepath || num_weights == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_export: required parameter is NULL (ctx, weights, params, filepath)");
        return -1;
    }

    /* Export would serialize quantized weights and parameters */
    /* For now, just validate inputs */

    if (ctx->config.verbose) {
        printf("[QAT] Exporting %u quantized weights to %s\n", num_weights, filepath);
    }

    return 0;
}

//=============================================================================
// Integration API Implementation
//=============================================================================

int qat_connect_brain_factory(qat_ctx_t* ctx, void* brain_factory) {
    if (!ctx || !brain_factory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_connect_brain_factory: required parameter is NULL (ctx, brain_factory)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->brain_factory = brain_factory;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

int qat_get_stats(const qat_ctx_t* ctx, qat_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ctx->mutex);
    memcpy(stats, &ctx->stats, sizeof(qat_stats_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)ctx->mutex);

    return 0;
}

void qat_reset_stats(qat_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    nimcp_mutex_lock(ctx->mutex);
    memset(&ctx->stats, 0, sizeof(qat_stats_t));
    nimcp_mutex_unlock(ctx->mutex);
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

const char* qat_dtype_name(qat_dtype_t dtype) {
    if (dtype >= QAT_DTYPE_COUNT) {
        return "Unknown";
    }
    return dtype_names[dtype];
}

uint32_t qat_dtype_bits(qat_dtype_t dtype) {
    if (dtype >= QAT_DTYPE_COUNT) {
        return 0;
    }
    return dtype_bits[dtype];
}

const char* qat_scheme_name(qat_scheme_t scheme) {
    if (scheme >= QAT_SCHEME_COUNT) {
        return "Unknown";
    }
    return scheme_names[scheme];
}

int qat_validate_config(const qat_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Validate dtypes */
    if (config->default_weight_dtype >= QAT_DTYPE_COUNT ||
        config->default_activation_dtype >= QAT_DTYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_validate_config: config is NULL");
        return -1;
    }

    /* Validate scheme */
    if (config->default_scheme >= QAT_SCHEME_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "qat_validate_config: capacity exceeded");
        return -1;
    }

    /* Validate granularity */
    if (config->default_granularity >= QAT_GRANULARITY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "qat_validate_config: capacity exceeded");
        return -1;
    }

    /* Validate observer method */
    if (config->observer.method >= QAT_OBSERVER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "qat_validate_config: capacity exceeded");
        return -1;
    }

    /* Validate fake quant method */
    if (config->fake_quant.method >= QAT_FAKE_QUANT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "qat_validate_config: capacity exceeded");
        return -1;
    }

    /* Validate EMA decay */
    if (config->observer.ema_decay < 0.0f || config->observer.ema_decay > 1.0f) {
        return -1;
    }

    return 0;
}

float qat_compute_mse(
    const nimcp_tensor_t* original,
    const nimcp_tensor_t* quantized
) {
    if (!original || !quantized) {
        return 0.0f;
    }

    size_t count = nimcp_tensor_numel(original);
    size_t quant_count = nimcp_tensor_numel(quantized);

    if (count != quant_count || count == 0) {
        return 0.0f;
    }

    const float* orig_data = nimcp_tensor_data((nimcp_tensor_t*)original);
    const float* quant_data = nimcp_tensor_data((nimcp_tensor_t*)quantized);

    if (!orig_data || !quant_data) {
        return 0.0f;
    }

    float mse = 0.0f;
    for (size_t i = 0; i < count; i++) {
        float diff = orig_data[i] - quant_data[i];
        mse += diff * diff;
    }

    return mse / (float)count;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get quantization range for dtype
 */
static void get_qmin_qmax(qat_dtype_t dtype, int32_t* qmin, int32_t* qmax) {
    switch (dtype) {
        case QAT_DTYPE_INT8:
            *qmin = -128;
            *qmax = 127;
            break;
        case QAT_DTYPE_UINT8:
            *qmin = 0;
            *qmax = 255;
            break;
        case QAT_DTYPE_INT4:
            *qmin = -8;
            *qmax = 7;
            break;
        case QAT_DTYPE_INT2:
            *qmin = -2;
            *qmax = 1;
            break;
        case QAT_DTYPE_INT1:
            *qmin = -1;
            *qmax = 1;
            break;
        default:
            *qmin = -128;
            *qmax = 127;
            break;
    }
}

/**
 * @brief Compute scale and zero point from observed range
 */
static void compute_scale_zp(
    float min_val,
    float max_val,
    qat_dtype_t dtype,
    qat_scheme_t scheme,
    float* scale,
    int32_t* zero_point
) {
    int32_t qmin, qmax;
    get_qmin_qmax(dtype, &qmin, &qmax);

    float range = max_val - min_val;
    if (range < 1e-8f) {
        range = 1e-8f;
    }

    switch (scheme) {
        case QAT_SCHEME_SYMMETRIC:
            /* Symmetric: zero_point = 0, scale based on max absolute value */
            {
                float max_abs = fmaxf(fabsf(min_val), fabsf(max_val));
                if (max_abs < 1e-8f) max_abs = 1e-8f;
                *scale = max_abs / (float)qmax;
                *zero_point = 0;
            }
            break;

        case QAT_SCHEME_AFFINE:
            /* Affine: full range utilization */
            *scale = range / (float)(qmax - qmin);
            *zero_point = qmin - (int32_t)roundf(min_val / *scale);
            break;

        case QAT_SCHEME_POWER_OF_TWO:
            /* Power-of-2 scale for efficient shift operations */
            {
                float max_abs = fmaxf(fabsf(min_val), fabsf(max_val));
                float log_scale = log2f(max_abs / (float)qmax);
                int32_t exp = (int32_t)ceilf(log_scale);
                *scale = powf(2.0f, (float)exp);
                *zero_point = 0;
            }
            break;

        default:
            *scale = range / (float)(qmax - qmin);
            *zero_point = 0;
            break;
    }

    /* Ensure scale is positive */
    if (*scale < 1e-10f) {
        *scale = 1e-10f;
    }
}

//=============================================================================
// Ternary Quantization Implementation
//=============================================================================

int qat_ternary_default_config(qat_ternary_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(qat_ternary_config_t));

    config->threshold_ratio = 0.7f;
    config->use_learned_threshold = false;
    config->initial_threshold = 0.0f;
    config->symmetric = true;
    config->use_ste = true;
    config->ste_gradient_scale = 1.0f;
    config->normalize_weights = true;

    return 0;
}

qat_ctx_t* qat_ternary_create(const qat_ternary_config_t* ternary_config) {
    if (!ternary_config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ternary_config is NULL");

        return NULL;
    }

    /* Create base QAT config with ternary dtype */
    qat_config_t base_config;
    if (qat_default_config(&base_config) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qat_ternary_create: validation failed");
        return NULL;
    }

    /* Set ternary-specific settings */
    base_config.default_weight_dtype = QAT_DTYPE_TERNARY;
    base_config.default_activation_dtype = QAT_DTYPE_INT8;  /* Keep activations INT8 */
    base_config.default_scheme = QAT_SCHEME_SYMMETRIC;

    /* Use STE for fake quantization */
    base_config.fake_quant.method = QAT_FAKE_QUANT_STE;
    base_config.fake_quant.grad_scale_factor = ternary_config->ste_gradient_scale;

    return qat_create(&base_config);
}

int qat_ternarize(
    qat_ctx_t* ctx,
    nimcp_tensor_t* tensor,
    const qat_ternary_config_t* ternary_config,
    qat_ternary_params_t* params
) {
    if (!ctx || !tensor || !ternary_config || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_ternarize: required parameter is NULL (ctx, tensor, ternary_config, params)");
        return -1;
    }

    size_t count = nimcp_tensor_numel(tensor);
    float* data = nimcp_tensor_data(tensor);

    if (!data || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qat_ternarize: data is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Initialize parameters */
    memset(params, 0, sizeof(qat_ternary_params_t));

    /* Compute mean absolute value for threshold */
    float sum_abs = 0.0f;
    float max_abs = 0.0f;
    for (size_t i = 0; i < count; i++) {
        float abs_val = fabsf(data[i]);
        sum_abs += abs_val;
        if (abs_val > max_abs) {
            max_abs = abs_val;
        }
    }
    float mean_abs = sum_abs / (float)count;

    /* Compute threshold */
    float threshold;
    if (ternary_config->use_learned_threshold && ternary_config->initial_threshold > 0.0f) {
        threshold = ternary_config->initial_threshold;
    } else {
        threshold = ternary_config->threshold_ratio * mean_abs;
    }

    /* Normalize weights if configured */
    float* normalized = data;
    float norm_factor = 1.0f;
    if (ternary_config->normalize_weights && max_abs > 1e-8f) {
        norm_factor = max_abs;
        /* Scale threshold accordingly */
        threshold = threshold / max_abs;
    }

    /* Ternarize: apply threshold-based quantization */
    float sum_positive = 0.0f;
    float sum_negative = 0.0f;
    uint64_t n_positive = 0;
    uint64_t n_negative = 0;
    uint64_t n_zero = 0;

    for (size_t i = 0; i < count; i++) {
        float w = ternary_config->normalize_weights ? (data[i] / norm_factor) : data[i];

        if (w > threshold) {
            sum_positive += w;
            n_positive++;
        } else if (w < -threshold) {
            sum_negative += fabsf(w);
            n_negative++;
        } else {
            n_zero++;
        }
    }

    /* Compute scale factors (mean of non-zero values in each direction) */
    float scale_positive = (n_positive > 0) ? (sum_positive / (float)n_positive) : 1.0f;
    float scale_negative = (n_negative > 0) ? (sum_negative / (float)n_negative) : 1.0f;

    /* Apply symmetric scaling if configured */
    if (ternary_config->symmetric) {
        float avg_scale = (scale_positive + scale_negative) / 2.0f;
        scale_positive = avg_scale;
        scale_negative = avg_scale;
    }

    /* Denormalize scales */
    if (ternary_config->normalize_weights) {
        scale_positive *= norm_factor;
        scale_negative *= norm_factor;
        threshold *= norm_factor;
    }

    /* Apply ternarization to tensor data */
    for (size_t i = 0; i < count; i++) {
        float w = data[i];

        if (w > threshold) {
            data[i] = scale_positive;  /* +1 * scale */
        } else if (w < -threshold) {
            data[i] = -scale_negative;  /* -1 * scale */
        } else {
            data[i] = 0.0f;  /* 0 */
        }
    }

    /* Store parameters */
    params->positive_scale = scale_positive;
    params->negative_scale = scale_negative;
    params->threshold = threshold;
    params->learned_threshold = threshold;
    params->n_positive = n_positive;
    params->n_zero = n_zero;
    params->n_negative = n_negative;
    params->sparsity = (float)n_zero / (float)count;

    /* Update statistics */
    ctx->stats.total_steps++;

    if (ctx->config.verbose) {
        printf("[QAT-Ternary] Ternarized tensor: +1=%lu, 0=%lu, -1=%lu (sparsity=%.2f%%)\n",
               (unsigned long)n_positive, (unsigned long)n_zero, (unsigned long)n_negative,
               params->sparsity * 100.0f);
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int qat_ternary_backward(
    qat_ctx_t* ctx,
    const nimcp_tensor_t* grad_output,
    const nimcp_tensor_t* original_weights,
    const qat_ternary_config_t* ternary_config,
    nimcp_tensor_t* grad_input
) {
    if (!ctx || !grad_output || !original_weights || !ternary_config || !grad_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_ternary_backward: required parameter is NULL (ctx, grad_output, original_weights, ternary_config, grad_input)");
        return -1;
    }

    size_t count = nimcp_tensor_numel(grad_output);
    size_t weight_count = nimcp_tensor_numel(original_weights);
    size_t grad_count = nimcp_tensor_numel(grad_input);

    if (count != weight_count || count != grad_count || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qat_ternary_backward: count is zero");
        return -1;
    }

    const float* grad_out_data = nimcp_tensor_data((nimcp_tensor_t*)grad_output);
    const float* weight_data = nimcp_tensor_data((nimcp_tensor_t*)original_weights);
    float* grad_in_data = nimcp_tensor_data(grad_input);

    if (!grad_out_data || !weight_data || !grad_in_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_ternary_backward: required parameter is NULL (grad_out_data, weight_data, grad_in_data)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (ternary_config->use_ste) {
        /* Straight-Through Estimator: pass gradient unchanged */
        /* This is the standard approach for ternary networks */
        float scale = ternary_config->ste_gradient_scale;

        for (size_t i = 0; i < count; i++) {
            grad_in_data[i] = grad_out_data[i] * scale;
        }
    } else {
        /* Alternative: clipped gradient (zero gradient for values outside clip range) */
        /* Compute threshold from weights */
        float sum_abs = 0.0f;
        for (size_t i = 0; i < count; i++) {
            sum_abs += fabsf(weight_data[i]);
        }
        float mean_abs = sum_abs / (float)count;
        float threshold = ternary_config->threshold_ratio * mean_abs;

        /* Clipped STE: zero gradient for values far from zero */
        float clip_bound = threshold * 2.0f;  /* Allow some margin */

        for (size_t i = 0; i < count; i++) {
            if (fabsf(weight_data[i]) <= clip_bound) {
                grad_in_data[i] = grad_out_data[i] * ternary_config->ste_gradient_scale;
            } else {
                grad_in_data[i] = 0.0f;
            }
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int qat_compute_optimal_ternary_threshold(
    const nimcp_tensor_t* tensor,
    float* threshold_out,
    float* scale_out
) {
    if (!tensor || !threshold_out || !scale_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_compute_optimal_ternary_threshold: required parameter is NULL (tensor, threshold_out, scale_out)");
        return -1;
    }

    size_t count = nimcp_tensor_numel(tensor);
    const float* data = nimcp_tensor_data((nimcp_tensor_t*)tensor);

    if (!data || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qat_compute_optimal_ternary_threshold: data is NULL");
        return -1;
    }

    /* Compute statistics */
    float sum = 0.0f;
    float sum_sq = 0.0f;
    float max_abs = 0.0f;

    for (size_t i = 0; i < count; i++) {
        float abs_val = fabsf(data[i]);
        sum += abs_val;
        sum_sq += abs_val * abs_val;
        if (abs_val > max_abs) {
            max_abs = abs_val;
        }
    }

    float mean_abs = sum / (float)count;
    float variance = fmaxf(0.0f, sum_sq / (float)count - mean_abs * mean_abs);
    float std_abs = sqrtf(variance);

    /* Optimal threshold approximation based on Ternary Weight Networks paper:
     * threshold ~= 0.7 * mean(|W|)
     * This minimizes quantization error for typical weight distributions
     */
    float threshold = 0.7f * mean_abs;

    /* Refine threshold based on variance:
     * If high variance, use slightly higher threshold to increase sparsity
     */
    if (std_abs > mean_abs * 0.5f) {
        threshold = mean_abs;  /* Higher threshold for high-variance weights */
    }

    /* Compute optimal scale as mean of non-zero ternarized values */
    float sum_positive = 0.0f;
    uint64_t n_positive = 0;

    for (size_t i = 0; i < count; i++) {
        if (fabsf(data[i]) > threshold) {
            sum_positive += fabsf(data[i]);
            n_positive++;
        }
    }

    float scale = (n_positive > 0) ? (sum_positive / (float)n_positive) : 1.0f;

    *threshold_out = threshold;
    *scale_out = scale;

    return 0;
}

int qat_get_ternary_stats(
    const qat_ctx_t* ctx,
    qat_ternary_params_t* params
) {
    if (!ctx || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qat_get_ternary_stats: required parameter is NULL (ctx, params)");
        return -1;
    }

    /* This would retrieve stored ternary statistics from the context */
    /* For now, just clear the params as statistics are computed per-ternarization */
    memset(params, 0, sizeof(qat_ternary_params_t));

    return 0;
}
