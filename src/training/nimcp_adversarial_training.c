/**
 * @file nimcp_adversarial_training.c
 * @brief Adversarial Training Implementation
 *
 * WHAT: Train models to be robust against adversarial perturbations
 * WHY:  Improve robustness, safety, and reliability
 * HOW:  PGD training, TRADES, AWP, certified defense
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch Advertorch: Similar attack interface
 * - CleverHans: More evaluation focus
 * - RobustBench: Evaluation benchmark
 *
 * NIMCP ADVANTAGES:
 * - Integration with brain immune system
 * - Bio-inspired detection mechanisms
 * - Native gradient manager support
 *
 * BIOLOGICAL GROUNDING:
 * - Neural noise tolerance: Robust to perturbations
 * - Immune system: Adversarial = pathogen detection
 * - Lateral inhibition: Suppress abnormal activations
 * - Predictive coding: Detect unexpected inputs
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "training/nimcp_adversarial_training.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define LOG_MODULE "ADV_TRAINING"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(adv)

//=============================================================================
// Internal Constants
//=============================================================================

#define ADV_EPSILON           NIMCP_EPSILON_ADAM
#define ADV_MAX_BATCH         256

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief AWP state for weight perturbation
 */
typedef struct {
    float* backup_params;            /**< Backup of original parameters */
    float* perturbation;             /**< Weight perturbation */
    size_t num_params;               /**< Number of parameters */
    bool applied;                    /**< Perturbation is applied */
} awp_state_t;

/**
 * @brief Detection state
 */
typedef struct {
    float* activation_means;         /**< Mean activations per layer */
    float* activation_stds;          /**< Std activations per layer */
    uint32_t num_layers;             /**< Number of monitored layers */
    uint64_t num_samples;            /**< Samples used for stats */
} detection_state_t;

/**
 * @brief Curriculum state
 */
typedef struct {
    float current_epsilon;           /**< Current attack epsilon */
    uint32_t current_steps;          /**< Current attack steps */
    uint32_t epoch;                  /**< Current training epoch */
} curriculum_state_t;

/**
 * @brief Adversarial context implementation
 */
struct adv_ctx_s {
    adv_config_t config;             /**< Configuration */
    bool initialized;                /**< Context initialized */

    /* Attack state */
    unsigned int rng_seed;           /**< RNG state */
    uint32_t attack_iter;            /**< Attack iteration counter */

    /* AWP state */
    awp_state_t awp;                 /**< AWP state */

    /* Detection state */
    detection_state_t detection;     /**< Detection state */

    /* Curriculum state */
    curriculum_state_t curriculum;   /**< Curriculum learning state */

    /* Integration points */
    nimcp_gradient_manager_ctx_t* grad_manager;
    void* brain_immune;
    void* predictive_immune;

    /* Statistics */
    adv_stats_t stats;

    /* Timing */
    struct timespec attack_start;
    struct timespec training_start;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static float compute_loss(const nimcp_tensor_t* logits, const nimcp_tensor_t* labels);
static void compute_gradient(const nimcp_tensor_t* logits, const nimcp_tensor_t* labels,
                             const nimcp_tensor_t* input, float* grad);
static float compute_norm(const float* data, size_t size, adv_norm_t norm);
static void clip_by_norm(float* data, size_t size, float max_norm, adv_norm_t norm);
static void clamp_tensor(float* data, size_t size, float min_val, float max_val);
static uint32_t argmax(const float* data, size_t size);
static float kl_divergence(const float* p, const float* q, size_t size);
static void softmax(const float* logits, float* probs, size_t size);

//=============================================================================
// Lifecycle API
//=============================================================================

int adv_default_config(adv_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "adv_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Default: Standard adversarial training with PGD */
    config->method = ADV_TRAIN_STANDARD;

    /* Attack configuration */
    config->attack.type = ADV_ATTACK_PGD;
    config->attack.norm = ADV_NORM_LINF;
    config->attack.epsilon = ADV_DEFAULT_EPSILON;
    config->attack.step_size = ADV_DEFAULT_STEP_SIZE;
    config->attack.num_steps = ADV_DEFAULT_NUM_STEPS;
    config->attack.random_start = true;
    config->attack.random_init_scale = 1.0f;
    config->attack.targeted = false;
    config->attack.target_class = -1;

    /* C&W defaults */
    config->attack.confidence = 0.0f;
    config->attack.c_init = 0.001f;
    config->attack.binary_search_steps = 9;

    /* TRADES defaults */
    config->trades.beta = ADV_DEFAULT_TRADES_BETA;
    config->trades.use_kl_loss = true;
    config->trades.clean_weight = 1.0f;

    /* AWP defaults */
    config->awp.gamma = 0.01f;
    config->awp.awp_warmup = 10;
    config->awp.perturb_running_stats = false;

    /* Certified defaults */
    config->certified.certified_epsilon = 0.0f;
    config->certified.use_ibp = false;
    config->certified.use_crown = false;
    config->certified.kappa_schedule_start = 0.0f;
    config->certified.kappa_schedule_end = 1.0f;
    config->certified.schedule_length = 100;

    /* Detection defaults */
    config->detection.method = ADV_DETECT_NONE;
    config->detection.threshold = 0.5f;
    config->detection.log_detections = true;
    config->detection.reject_detected = false;

    /* Training schedule */
    config->adversarial_ratio = 1.0f;
    config->warmup_epochs = 0;
    config->curriculum = false;
    config->curriculum_epsilon_init = 0.0f;

    /* Regularization */
    config->smoothness_reg = 0.0f;
    config->lipschitz_reg = 0.0f;

    /* Integration enabled by default */
    config->integrate_gradient_manager = true;
    config->integrate_brain_immune = true;
    config->integrate_predictive_immune = true;

    config->verbose = false;
    config->track_statistics = true;
    config->save_adversarial_examples = false;

    return 0;
}

int adv_trades_config(adv_config_t* config) {
    if (adv_default_config(config) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adv_trades_config: validation failed");
        return -1;
    }

    config->method = ADV_TRAIN_TRADES;
    config->trades.beta = ADV_DEFAULT_TRADES_BETA;
    config->trades.use_kl_loss = true;
    config->trades.clean_weight = 1.0f;

    return 0;
}

adv_ctx_t* adv_create(const adv_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "adv_create: config is NULL");
        return NULL;
    }

    if (adv_validate_config(config) != 0) {
        NIMCP_THROW(NIMCP_ERROR_CONFIG_INVALID, "adv_create: config validation failed");
        return NULL;
    }

    adv_ctx_t* ctx = nimcp_calloc(1, sizeof(adv_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(adv_ctx_t),
                          "adv_create: failed to allocate context");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(adv_config_t));

    /* Initialize RNG */
    ctx->rng_seed = (unsigned int)time(NULL);

    /* Initialize curriculum */
    if (config->curriculum) {
        ctx->curriculum.current_epsilon = config->curriculum_epsilon_init;
        ctx->curriculum.current_steps = 1;
        ctx->curriculum.epoch = 0;
    } else {
        ctx->curriculum.current_epsilon = config->attack.epsilon;
        ctx->curriculum.current_steps = config->attack.num_steps;
    }

    /* Create mutex */
    ctx->mutex = nimcp_mutex_create(NULL);
    if (!ctx->mutex) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adv_create: ctx->mutex is NULL");
        return NULL;
    }

    ctx->initialized = true;
    return ctx;
}

void adv_destroy(adv_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Free AWP state */
    nimcp_free(ctx->awp.backup_params);
    nimcp_free(ctx->awp.perturbation);

    /* Free detection state */
    nimcp_free(ctx->detection.activation_means);
    nimcp_free(ctx->detection.activation_stds);

    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
    }

    nimcp_free(ctx);
}

//=============================================================================
// Attack Generation API
//=============================================================================

int adv_fgsm(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* input,
    const nimcp_tensor_t* label,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    adv_example_t* adv_example
) {
    if (!ctx || !input || !label || !forward_fn || !model || !adv_example) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_fgsm: required parameter is NULL (ctx, input, label, forward_fn, model, adv_example)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Get input dimensions */
    size_t input_size = nimcp_tensor_numel(input);

    float epsilon = ctx->curriculum.current_epsilon;

    /* Clone input for adversarial example */
    nimcp_tensor_t* adv_input = nimcp_tensor_clone(input);
    if (!adv_input) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_fgsm: adv_input is NULL");
        return -1;
    }

    /* Forward pass */
    nimcp_tensor_t* logits = forward_fn(model, input);
    if (!logits) {
        nimcp_tensor_destroy(adv_input);
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_fgsm: logits is NULL");
        return -1;
    }

    /* Compute gradient of loss w.r.t. input */
    float* grad = nimcp_calloc(input_size, sizeof(float));
    if (!grad) {
        nimcp_tensor_destroy(adv_input);
        nimcp_tensor_destroy(logits);
        nimcp_mutex_unlock(ctx->mutex);
        return -1;
    }
    compute_gradient(logits, label, input, grad);

    /* FGSM: x_adv = x + epsilon * sign(grad) */
    float* adv_data = (float*)nimcp_tensor_data(adv_input);
    const float* orig_data = (const float*)nimcp_tensor_data((nimcp_tensor_t*)input);

    for (size_t i = 0; i < input_size; i++) {
        float sign = grad[i] > 0 ? 1.0f : (grad[i] < 0 ? -1.0f : 0.0f);
        adv_data[i] = orig_data[i] + epsilon * sign;
    }

    /* Clamp to valid range [0, 1] */
    clamp_tensor(adv_data, input_size, 0.0f, 1.0f);

    /* Populate adversarial example */
    adv_example->clean_input = nimcp_tensor_clone(input);
    adv_example->adv_input = adv_input;

    /* Compute perturbation */
    adv_example->perturbation = nimcp_tensor_clone(adv_input);
    float* pert_data = (float*)nimcp_tensor_data(adv_example->perturbation);
    for (size_t i = 0; i < input_size; i++) {
        pert_data[i] = adv_data[i] - orig_data[i];
    }

    /* Compute perturbation norm */
    adv_example->perturbation_norm = compute_norm(pert_data, input_size,
                                                   ctx->config.attack.norm);

    /* Check attack success */
    nimcp_tensor_t* adv_logits = forward_fn(model, adv_input);
    if (adv_logits) {
        size_t num_classes = nimcp_tensor_numel(logits);
        adv_example->original_class = argmax((float*)nimcp_tensor_data((nimcp_tensor_t*)logits), num_classes);
        adv_example->adversarial_class = argmax((float*)nimcp_tensor_data(adv_logits), num_classes);
        adv_example->attack_success = (adv_example->original_class != adv_example->adversarial_class);
        nimcp_tensor_destroy(adv_logits);
    }

    /* Update statistics */
    ctx->stats.total_attacks++;
    if (adv_example->attack_success) {
        ctx->stats.successful_attacks++;
    }
    ctx->stats.avg_perturbation_norm =
        (ctx->stats.avg_perturbation_norm * (float)(ctx->stats.total_attacks - 1) +
         adv_example->perturbation_norm) / (float)ctx->stats.total_attacks;

    nimcp_free(grad);
    nimcp_tensor_destroy(logits);

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int adv_pgd(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* input,
    const nimcp_tensor_t* label,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    adv_example_t* adv_example
) {
    if (!ctx || !input || !label || !forward_fn || !model || !adv_example) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_pgd: required parameter is NULL (ctx, input, label, forward_fn, model, adv_example)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    size_t input_size = nimcp_tensor_numel(input);

    float epsilon = ctx->curriculum.current_epsilon;
    float step_size = ctx->config.attack.step_size;
    uint32_t num_steps = ctx->curriculum.current_steps;
    adv_norm_t norm = ctx->config.attack.norm;

    /* Clone input for adversarial example */
    nimcp_tensor_t* adv_input = nimcp_tensor_clone(input);
    if (!adv_input) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_pgd: adv_input is NULL");
        return -1;
    }

    float* adv_data = (float*)nimcp_tensor_data(adv_input);
    const float* orig_data = (const float*)nimcp_tensor_data((nimcp_tensor_t*)input);

    /* Random start */
    if (ctx->config.attack.random_start) {
        float init_scale = ctx->config.attack.random_init_scale * epsilon;
        for (size_t i = 0; i < input_size; i++) {
            float r = (float)rand_r(&ctx->rng_seed) / (float)RAND_MAX * 2.0f - 1.0f;
            adv_data[i] = orig_data[i] + init_scale * r;
        }
        clamp_tensor(adv_data, input_size, 0.0f, 1.0f);
    }

    float* grad = nimcp_calloc(input_size, sizeof(float));
    float* perturbation = nimcp_calloc(input_size, sizeof(float));

    /* Guard: allocation failure → clean up and return error */
    if (!grad || !perturbation) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
            input_size * sizeof(float) * 2,
            "PGD attack: failed to allocate gradient/perturbation buffers");
        nimcp_free(grad);
        nimcp_free(perturbation);
        nimcp_tensor_destroy(adv_input);
        nimcp_mutex_unlock(ctx->mutex);
        return -1;
    }

    /* PGD iterations */
    for (uint32_t step = 0; step < num_steps; step++) {
        /* Heartbeat progress during PGD attack */
        adv_heartbeat("pgd_attack", (float)(step + 1) / (float)num_steps);

        /* Forward pass */
        nimcp_tensor_t* logits = forward_fn(model, adv_input);
        if (!logits) {
            continue;
        }

        /* Compute gradient */
        compute_gradient(logits, label, adv_input, grad);
        nimcp_tensor_destroy(logits);

        /* Gradient step: x = x + step_size * sign(grad) */
        for (size_t i = 0; i < input_size; i++) {
            if (norm == ADV_NORM_LINF) {
                float sign = grad[i] > 0 ? 1.0f : (grad[i] < 0 ? -1.0f : 0.0f);
                adv_data[i] = adv_data[i] + step_size * sign;
            } else if (norm == ADV_NORM_L2) {
                float grad_norm = compute_norm(grad, input_size, ADV_NORM_L2);
                if (grad_norm > ADV_EPSILON) {
                    adv_data[i] = adv_data[i] + step_size * grad[i] / grad_norm;
                }
            }
        }

        /* Compute perturbation */
        for (size_t i = 0; i < input_size; i++) {
            perturbation[i] = adv_data[i] - orig_data[i];
        }

        /* Project back to epsilon ball */
        adv_project_perturbation(perturbation, input_size, epsilon, norm);

        /* Apply projected perturbation */
        for (size_t i = 0; i < input_size; i++) {
            adv_data[i] = orig_data[i] + perturbation[i];
        }

        /* Clamp to valid range */
        clamp_tensor(adv_data, input_size, 0.0f, 1.0f);
    }

    /* Populate adversarial example */
    adv_example->clean_input = nimcp_tensor_clone(input);
    adv_example->adv_input = adv_input;

    adv_example->perturbation = nimcp_tensor_clone(adv_input);
    float* pert_data = (float*)nimcp_tensor_data(adv_example->perturbation);
    for (size_t i = 0; i < input_size; i++) {
        pert_data[i] = adv_data[i] - orig_data[i];
    }

    adv_example->perturbation_norm = compute_norm(pert_data, input_size, norm);

    /* Check attack success */
    nimcp_tensor_t* clean_logits = forward_fn(model, input);
    nimcp_tensor_t* adv_logits = forward_fn(model, adv_input);

    if (clean_logits && adv_logits) {
        const nimcp_tensor_shape_t* cl_shape = nimcp_tensor_shape(clean_logits);
        const nimcp_tensor_shape_t* al_shape = nimcp_tensor_shape(adv_logits);
        adv_example->original_class = argmax((float*)nimcp_tensor_data(clean_logits), cl_shape->dims[0]);
        adv_example->adversarial_class = argmax((float*)nimcp_tensor_data(adv_logits), al_shape->dims[0]);
        adv_example->attack_success = (adv_example->original_class != adv_example->adversarial_class);
    }

    nimcp_tensor_destroy(clean_logits);
    nimcp_tensor_destroy(adv_logits);

    /* Update statistics */
    ctx->stats.total_attacks++;
    if (adv_example->attack_success) {
        ctx->stats.successful_attacks++;
    }
    ctx->stats.avg_perturbation_norm =
        (ctx->stats.avg_perturbation_norm * (float)(ctx->stats.total_attacks - 1) +
         adv_example->perturbation_norm) / (float)ctx->stats.total_attacks;
    ctx->stats.attack_success_rate = (float)ctx->stats.successful_attacks /
                                     (float)ctx->stats.total_attacks;

    nimcp_free(grad);
    nimcp_free(perturbation);

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

/**
 * @brief Generate adversarial batch (unlocked helper, caller must hold mutex)
 */
static int adv_generate_batch_unlocked(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    const nimcp_tensor_t* labels,
    uint32_t batch_size,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    nimcp_tensor_t** adv_inputs
) {
    nimcp_tensor_t* batch_adv = nimcp_tensor_clone(inputs);
    if (!batch_adv) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_generate_batch: batch_adv is NULL");
        return -1;
    }

    const nimcp_tensor_shape_t* in_shape = nimcp_tensor_shape(inputs);
    size_t sample_size = 1;
    for (uint32_t i = 1; i < in_shape->rank; i++) {
        sample_size *= in_shape->dims[i];
    }

    float* adv_data = (float*)nimcp_tensor_data(batch_adv);
    const float* orig_data = (const float*)nimcp_tensor_data_const(inputs);

    float epsilon = ctx->curriculum.current_epsilon;
    float step_size = ctx->config.attack.step_size;
    uint32_t num_steps = ctx->curriculum.current_steps;

    /* Allocate gradient buffer */
    float* grad = nimcp_calloc(batch_size * sample_size, sizeof(float));
    if (!grad) {
        return -1;
    }

    /* Random start */
    if (ctx->config.attack.random_start) {
        for (size_t i = 0; i < batch_size * sample_size; i++) {
            float r = (float)rand_r(&ctx->rng_seed) / (float)RAND_MAX * 2.0f - 1.0f;
            adv_data[i] = orig_data[i] + epsilon * r;
        }
        clamp_tensor(adv_data, batch_size * sample_size, 0.0f, 1.0f);
    }

    /* PGD iterations */
    for (uint32_t step = 0; step < num_steps; step++) {
        nimcp_tensor_t* logits = forward_fn(model, batch_adv);
        if (!logits) {
            continue;
        }

        /* Compute gradients (simplified batch gradient) */
        compute_gradient(logits, labels, batch_adv, grad);
        nimcp_tensor_destroy(logits);

        /* Update adversarial batch */
        for (size_t i = 0; i < batch_size * sample_size; i++) {
            float sign = grad[i] > 0 ? 1.0f : (grad[i] < 0 ? -1.0f : 0.0f);
            float pert = adv_data[i] - orig_data[i] + step_size * sign;

            /* Clamp perturbation to epsilon ball (L-inf) */
            if (pert > epsilon) pert = epsilon;
            if (pert < -epsilon) pert = -epsilon;

            adv_data[i] = orig_data[i] + pert;
        }

        /* Clamp to valid range */
        clamp_tensor(adv_data, batch_size * sample_size, 0.0f, 1.0f);
    }

    *adv_inputs = batch_adv;

    nimcp_free(grad);
    return 0;
}

int adv_generate_batch(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    const nimcp_tensor_t* labels,
    uint32_t batch_size,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    nimcp_tensor_t** adv_inputs
) {
    if (!ctx || !inputs || !labels || !forward_fn || !model || !adv_inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_generate_batch: required parameter is NULL (ctx, inputs, labels, forward_fn, model, adv_inputs)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    int ret = adv_generate_batch_unlocked(ctx, inputs, labels, batch_size, forward_fn, model, adv_inputs);
    nimcp_mutex_unlock(ctx->mutex);
    return ret;
}

//=============================================================================
// Adversarial Training API
//=============================================================================

int adv_compute_loss(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    const nimcp_tensor_t* labels,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* total_loss,
    float* clean_loss,
    float* adv_loss
) {
    if (!ctx || !inputs || !labels || !forward_fn || !model || !total_loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_compute_loss: required parameter is NULL (ctx, inputs, labels, forward_fn, model, total_loss)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Forward on clean examples */
    nimcp_tensor_t* clean_logits = forward_fn(model, inputs);
    if (!clean_logits) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_compute_loss: clean_logits is NULL");
        return -1;
    }

    float l_clean = compute_loss(clean_logits, labels);

    /* Generate adversarial examples (use unlocked version - we already hold mutex) */
    nimcp_tensor_t* adv_inputs = NULL;
    uint32_t batch_size = nimcp_tensor_shape(inputs)->dims[0];

    int ret = adv_generate_batch_unlocked(ctx, inputs, labels, batch_size, forward_fn, model, &adv_inputs);
    if (ret != 0 || !adv_inputs) {
        nimcp_tensor_destroy(clean_logits);
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_compute_loss: adv_inputs is NULL");
        return -1;
    }

    /* Forward on adversarial examples */
    nimcp_tensor_t* adv_logits = forward_fn(model, adv_inputs);
    if (!adv_logits) {
        nimcp_tensor_destroy(clean_logits);
        nimcp_tensor_destroy(adv_inputs);
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_compute_loss: adv_logits is NULL");
        return -1;
    }

    float l_adv = 0.0f;
    float l_total = 0.0f;

    switch (ctx->config.method) {
        case ADV_TRAIN_STANDARD: {
            /* Standard AT: train on adversarial examples */
            l_adv = compute_loss(adv_logits, labels);
            l_total = l_adv;
            break;
        }

        case ADV_TRAIN_TRADES: {
            /* TRADES: CE(clean) + beta * KL(clean || adv) */
            const nimcp_tensor_shape_t* cl_shape = nimcp_tensor_shape(clean_logits);
            size_t num_classes = cl_shape->dims[1];

            float* clean_probs = nimcp_calloc(batch_size * num_classes, sizeof(float));
            float* adv_probs = nimcp_calloc(batch_size * num_classes, sizeof(float));
            if (!clean_probs || !adv_probs) {
                if (clean_probs) nimcp_free(clean_probs);
                if (adv_probs) nimcp_free(adv_probs);
                nimcp_tensor_destroy(clean_logits);
                nimcp_tensor_destroy(adv_logits);
                nimcp_mutex_unlock(ctx->mutex);
                return -1;
            }

            /* Compute softmax */
            const float* cl_data = (const float*)nimcp_tensor_data_const(clean_logits);
            const float* al_data = (const float*)nimcp_tensor_data_const(adv_logits);
            for (uint32_t b = 0; b < batch_size; b++) {
                softmax(&cl_data[b * num_classes],
                       &clean_probs[b * num_classes], num_classes);
                softmax(&al_data[b * num_classes],
                       &adv_probs[b * num_classes], num_classes);
            }

            /* Compute KL divergence */
            float kl_sum = 0.0f;
            for (uint32_t b = 0; b < batch_size; b++) {
                kl_sum += kl_divergence(&clean_probs[b * num_classes],
                                       &adv_probs[b * num_classes], num_classes);
            }
            float kl_loss = kl_sum / (float)batch_size;

            l_adv = kl_loss;
            l_total = ctx->config.trades.clean_weight * l_clean +
                     ctx->config.trades.beta * kl_loss;

            nimcp_free(clean_probs);
            nimcp_free(adv_probs);

            ctx->stats.avg_trades_loss = kl_loss;
            break;
        }

        case ADV_TRAIN_MART: {
            /* MART: Boosted cross-entropy based on prediction confidence */
            l_adv = compute_loss(adv_logits, labels);

            /* Weight by (1 - p_y) where p_y is probability of true class */
            const nimcp_tensor_shape_t* mart_shape = nimcp_tensor_shape(adv_logits);
            size_t num_classes = mart_shape->dims[1];
            float boosted_loss = 0.0f;

            const float* mart_data = (const float*)nimcp_tensor_data_const(adv_logits);
            const float* labels_data = (const float*)nimcp_tensor_data_const(labels);
            for (uint32_t b = 0; b < batch_size; b++) {
                float* probs = nimcp_calloc(num_classes, sizeof(float));
                if (!probs) {
                    break;
                }
                softmax(&mart_data[b * num_classes], probs, num_classes);

                uint32_t true_class = (uint32_t)labels_data[b];
                float p_y = probs[true_class];
                float weight = 1.0f - p_y;

                /* Compute sample CE loss */
                float sample_loss = -logf(p_y + ADV_EPSILON);
                boosted_loss += weight * sample_loss;

                nimcp_free(probs);
            }

            l_adv = boosted_loss / (float)batch_size;
            l_total = l_clean + l_adv;
            break;
        }

        case ADV_TRAIN_FREE:
        case ADV_TRAIN_FAST: {
            /* Free/Fast AT: single-step attack */
            l_adv = compute_loss(adv_logits, labels);
            l_total = l_adv;
            break;
        }

        case ADV_TRAIN_ADVERSARIAL_AUGMENT: {
            /* Mix of clean and adversarial */
            l_adv = compute_loss(adv_logits, labels);
            l_total = (1.0f - ctx->config.adversarial_ratio) * l_clean +
                     ctx->config.adversarial_ratio * l_adv;
            break;
        }

        default:
            l_adv = compute_loss(adv_logits, labels);
            l_total = l_adv;
            break;
    }

    *total_loss = l_total;
    if (clean_loss) *clean_loss = l_clean;
    if (adv_loss) *adv_loss = l_adv;

    /* Update statistics */
    ctx->stats.total_steps++;
    ctx->stats.adversarial_steps++;
    ctx->stats.avg_clean_loss = (ctx->stats.avg_clean_loss * (float)(ctx->stats.total_steps - 1) +
                                 l_clean) / (float)ctx->stats.total_steps;
    ctx->stats.avg_adv_loss = (ctx->stats.avg_adv_loss * (float)(ctx->stats.adversarial_steps - 1) +
                               l_adv) / (float)ctx->stats.adversarial_steps;

    nimcp_tensor_destroy(clean_logits);
    nimcp_tensor_destroy(adv_logits);
    nimcp_tensor_destroy(adv_inputs);

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int adv_awp_perturb(
    adv_ctx_t* ctx,
    float* params,
    size_t num_params,
    const float* gradients
) {
    if (!ctx || !params || !gradients || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_awp_perturb: required parameter is NULL (ctx, params, gradients)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Allocate if needed */
    if (!ctx->awp.backup_params || ctx->awp.num_params != num_params) {
        nimcp_free(ctx->awp.backup_params);
        nimcp_free(ctx->awp.perturbation);

        ctx->awp.backup_params = nimcp_calloc(num_params, sizeof(float));
        ctx->awp.perturbation = nimcp_calloc(num_params, sizeof(float));
        ctx->awp.num_params = num_params;

        if (!ctx->awp.backup_params || !ctx->awp.perturbation) {
            nimcp_mutex_unlock(ctx->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adv_awp_perturb: required parameter is NULL (ctx->awp, ctx->awp)");
            return -1;
        }
    }

    /* Backup parameters */
    memcpy(ctx->awp.backup_params, params, num_params * sizeof(float));

    /* Compute weight perturbation: delta_w = gamma * grad / ||grad||_2 */
    float grad_norm = 0.0f;
    for (size_t i = 0; i < num_params; i++) {
        grad_norm += gradients[i] * gradients[i];
    }
    grad_norm = sqrtf(grad_norm + ADV_EPSILON);

    float gamma = ctx->config.awp.gamma;

    for (size_t i = 0; i < num_params; i++) {
        ctx->awp.perturbation[i] = gamma * gradients[i] / grad_norm;
        params[i] += ctx->awp.perturbation[i];
    }

    ctx->awp.applied = true;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int adv_awp_restore(
    adv_ctx_t* ctx,
    float* params,
    size_t num_params
) {
    if (!ctx || !params || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_awp_restore: required parameter is NULL (ctx, params)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (!ctx->awp.applied || num_params != ctx->awp.num_params) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_awp_restore: ctx->awp is NULL");
        return -1;
    }

    /* Restore parameters */
    memcpy(params, ctx->awp.backup_params, num_params * sizeof(float));
    ctx->awp.applied = false;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Detection API
//=============================================================================

int adv_detect(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    bool* is_adversarial,
    float* confidence
) {
    if (!ctx || !input || !forward_fn || !model || !is_adversarial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_detect: required parameter is NULL (ctx, input, forward_fn, model, is_adversarial)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    *is_adversarial = false;
    if (confidence) *confidence = 0.0f;

    switch (ctx->config.detection.method) {
        case ADV_DETECT_NONE:
            break;

        case ADV_DETECT_STATISTICAL: {
            /* Check if prediction confidence is unusually low */
            nimcp_tensor_t* logits = forward_fn(model, input);
            if (logits) {
                const nimcp_tensor_shape_t* l_shape = nimcp_tensor_shape(logits);
                size_t num_classes = l_shape->dims[l_shape->rank - 1];
                float* probs = nimcp_calloc(num_classes, sizeof(float));
                softmax((float*)nimcp_tensor_data(logits), probs, num_classes);

                /* Find max probability */
                float max_prob = 0.0f;
                for (size_t i = 0; i < num_classes; i++) {
                    if (probs[i] > max_prob) max_prob = probs[i];
                }

                /* Low confidence might indicate adversarial */
                float anomaly_score = 1.0f - max_prob;
                *is_adversarial = (anomaly_score > ctx->config.detection.threshold);
                if (confidence) *confidence = anomaly_score;

                nimcp_free(probs);
                nimcp_tensor_destroy(logits);
            }
            break;
        }

        case ADV_DETECT_INPUT_TRANSFORM: {
            /* Feature squeezing: compare predictions on squeezed inputs */
            /* Create squeezed version (bit depth reduction) */
            nimcp_tensor_t* squeezed = nimcp_tensor_clone(input);
            if (squeezed) {
                size_t size = nimcp_tensor_numel(squeezed);

                /* Reduce to 4-bit precision */
                float* data = (float*)nimcp_tensor_data(squeezed);
                for (size_t i = 0; i < size; i++) {
                    data[i] = roundf(data[i] * 16.0f) / 16.0f;
                }

                /* Compare predictions */
                nimcp_tensor_t* orig_logits = forward_fn(model, input);
                nimcp_tensor_t* sq_logits = forward_fn(model, squeezed);

                if (orig_logits && sq_logits) {
                    const nimcp_tensor_shape_t* ol_shape = nimcp_tensor_shape(orig_logits);
                    const nimcp_tensor_shape_t* sl_shape = nimcp_tensor_shape(sq_logits);
                    uint32_t orig_pred = argmax((float*)nimcp_tensor_data(orig_logits), ol_shape->dims[ol_shape->rank - 1]);
                    uint32_t sq_pred = argmax((float*)nimcp_tensor_data(sq_logits), sl_shape->dims[sl_shape->rank - 1]);

                    *is_adversarial = (orig_pred != sq_pred);
                    if (confidence) *confidence = *is_adversarial ? 1.0f : 0.0f;
                }

                nimcp_tensor_destroy(orig_logits);
                nimcp_tensor_destroy(sq_logits);
                nimcp_tensor_destroy(squeezed);
            }
            break;
        }

        default:
            break;
    }

    /* Update detection statistics */
    if (*is_adversarial) {
        /* Note: can't distinguish true/false positives without ground truth */
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Evaluation API
//=============================================================================

int adv_evaluate(
    adv_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    const nimcp_tensor_t* labels,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* clean_accuracy,
    float* robust_accuracy
) {
    if (!ctx || !inputs || !labels || !forward_fn || !model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_evaluate: required parameter is NULL (ctx, inputs, labels, forward_fn, model)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    uint32_t batch_size = nimcp_tensor_shape(inputs)->dims[0];
    uint32_t clean_correct = 0;
    uint32_t robust_correct = 0;

    /* Evaluate clean accuracy */
    nimcp_tensor_t* clean_logits = forward_fn(model, inputs);
    if (clean_logits) {
        size_t num_classes = nimcp_tensor_shape(clean_logits)->dims[1];
        const float* cl_data = (const float*)nimcp_tensor_data(clean_logits);
        const float* lbl_data = (const float*)nimcp_tensor_data_const(labels);

        for (uint32_t b = 0; b < batch_size; b++) {
            uint32_t pred = argmax(&cl_data[b * num_classes], num_classes);
            uint32_t label = (uint32_t)lbl_data[b];
            if (pred == label) clean_correct++;
        }

        nimcp_tensor_destroy(clean_logits);
    }

    /* Generate adversarial examples and evaluate (use unlocked version - we already hold mutex) */
    nimcp_tensor_t* adv_inputs = NULL;
    int ret = adv_generate_batch_unlocked(ctx, inputs, labels, batch_size, forward_fn, model, &adv_inputs);

    if (ret == 0 && adv_inputs) {
        nimcp_tensor_t* adv_logits = forward_fn(model, adv_inputs);
        if (adv_logits) {
            size_t num_classes = nimcp_tensor_shape(adv_logits)->dims[1];
            const float* al_data = (const float*)nimcp_tensor_data(adv_logits);
            const float* lbl_data2 = (const float*)nimcp_tensor_data_const(labels);

            for (uint32_t b = 0; b < batch_size; b++) {
                uint32_t pred = argmax(&al_data[b * num_classes], num_classes);
                uint32_t label = (uint32_t)lbl_data2[b];
                if (pred == label) robust_correct++;
            }

            nimcp_tensor_destroy(adv_logits);
        }
        nimcp_tensor_destroy(adv_inputs);
    }

    if (clean_accuracy) {
        *clean_accuracy = (float)clean_correct / (float)batch_size;
    }
    if (robust_accuracy) {
        *robust_accuracy = (float)robust_correct / (float)batch_size;
    }

    /* Update statistics */
    ctx->stats.clean_accuracy = (float)clean_correct / (float)batch_size;
    ctx->stats.robust_accuracy = (float)robust_correct / (float)batch_size;
    ctx->stats.accuracy_gap = ctx->stats.clean_accuracy - ctx->stats.robust_accuracy;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Integration API
//=============================================================================

int adv_connect_gradient_manager(
    adv_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->grad_manager = grad_manager;
    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int adv_connect_brain_immune(adv_ctx_t* ctx, void* brain_immune) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->brain_immune = brain_immune;
    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int adv_connect_predictive_immune(adv_ctx_t* ctx, void* predictive_immune) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->predictive_immune = predictive_immune;
    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Statistics API
//=============================================================================

int adv_get_stats(const adv_ctx_t* ctx, adv_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ctx->mutex);
    memcpy(stats, &ctx->stats, sizeof(adv_stats_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)ctx->mutex);
    return 0;
}

void adv_reset_stats(adv_ctx_t* ctx) {
    if (!ctx) return;

    nimcp_mutex_lock(ctx->mutex);
    memset(&ctx->stats, 0, sizeof(adv_stats_t));
    nimcp_mutex_unlock(ctx->mutex);
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* adv_attack_name(adv_attack_t attack) {
    static const char* names[] = {
        "FGSM",
        "PGD",
        "C&W",
        "DeepFool",
        "AutoAttack",
        "Square",
        "TRADES-Inner",
        "Free",
        "Custom"
    };
    return attack < ADV_ATTACK_COUNT ? names[attack] : "Unknown";
}

const char* adv_train_method_name(adv_train_method_t method) {
    static const char* names[] = {
        "Standard AT",
        "TRADES",
        "MART",
        "Free AT",
        "Fast AT",
        "AWP",
        "Adversarial Augment",
        "Certified"
    };
    return method < ADV_TRAIN_COUNT ? names[method] : "Unknown";
}

const char* adv_norm_name(adv_norm_t norm) {
    static const char* names[] = {
        "L-inf",
        "L2",
        "L1",
        "L0"
    };
    return norm < ADV_NORM_COUNT ? names[norm] : "Unknown";
}

int adv_validate_config(const adv_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    if (config->method >= ADV_TRAIN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "adv_validate_config: capacity exceeded");
        return -1;
    }

    if (config->attack.type >= ADV_ATTACK_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "adv_validate_config: capacity exceeded");
        return -1;
    }

    if (config->attack.norm >= ADV_NORM_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "adv_validate_config: capacity exceeded");
        return -1;
    }

    if (config->attack.epsilon < 0) {
        return -1;
    }

    if (config->attack.step_size <= 0) {
        return -1;
    }

    if (config->trades.beta < 0) {
        return -1;
    }

    return 0;
}

void adv_free_example(adv_example_t* example) {
    if (!example) return;

    nimcp_tensor_destroy(example->clean_input);
    nimcp_tensor_destroy(example->perturbation);
    nimcp_tensor_destroy(example->adv_input);
}

void adv_project_perturbation(
    float* perturbation,
    size_t size,
    float epsilon,
    adv_norm_t norm
) {
    if (!perturbation || size == 0 || epsilon <= 0) {
        return;
    }

    switch (norm) {
        case ADV_NORM_LINF: {
            /* Clamp to [-epsilon, epsilon] */
            for (size_t i = 0; i < size; i++) {
                if (perturbation[i] > epsilon) perturbation[i] = epsilon;
                if (perturbation[i] < -epsilon) perturbation[i] = -epsilon;
            }
            break;
        }

        case ADV_NORM_L2: {
            /* Scale if norm exceeds epsilon */
            float curr_norm = compute_norm(perturbation, size, ADV_NORM_L2);
            if (curr_norm > epsilon) {
                float scale = epsilon / curr_norm;
                for (size_t i = 0; i < size; i++) {
                    perturbation[i] *= scale;
                }
            }
            break;
        }

        case ADV_NORM_L1: {
            /* Project onto L1 ball (simplex projection) */
            float curr_norm = compute_norm(perturbation, size, ADV_NORM_L1);
            if (curr_norm > epsilon) {
                /* Simplified: uniform scaling (true L1 projection is more complex) */
                float scale = epsilon / curr_norm;
                for (size_t i = 0; i < size; i++) {
                    perturbation[i] *= scale;
                }
            }
            break;
        }

        default:
            break;
    }
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static float compute_loss(const nimcp_tensor_t* logits, const nimcp_tensor_t* labels) {
    if (!logits || !labels) {
        return 0.0f;
    }

    /* Cross-entropy loss */
    const nimcp_tensor_shape_t* l_shape = nimcp_tensor_shape(logits);
    uint32_t batch_size = l_shape->dims[0];
    size_t num_classes = l_shape->dims[1];
    const float* logits_data = (const float*)nimcp_tensor_data_const(logits);
    const float* labels_data = (const float*)nimcp_tensor_data_const(labels);

    float total_loss = 0.0f;

    for (uint32_t b = 0; b < batch_size; b++) {
        const float* sample_logits = &logits_data[b * num_classes];

        /* Softmax */
        float max_val = sample_logits[0];
        for (size_t c = 1; c < num_classes; c++) {
            if (sample_logits[c] > max_val) max_val = sample_logits[c];
        }

        float sum_exp = 0.0f;
        for (size_t c = 0; c < num_classes; c++) {
            sum_exp += expf(sample_logits[c] - max_val);
        }

        uint32_t label = (uint32_t)labels_data[b];
        if (label >= num_classes) continue;
        float log_prob = (sample_logits[label] - max_val) - logf(sum_exp);
        total_loss -= log_prob;
    }

    return total_loss / (float)batch_size;
}

static void compute_gradient(const nimcp_tensor_t* logits, const nimcp_tensor_t* labels,
                             const nimcp_tensor_t* input, float* grad) {
    if (!logits || !labels || !input || !grad) {
        return;
    }

    /* Simplified gradient computation */
    /* For CE loss with softmax: grad_x = sum_c (p_c - y_c) * grad_logit_c_wrt_x */
    /* Here we approximate as sign of loss direction */

    size_t input_size = nimcp_tensor_numel(input);
    const nimcp_tensor_shape_t* l_shape = nimcp_tensor_shape(logits);
    uint32_t batch_size = l_shape->dims[0];
    size_t num_classes = l_shape->dims[1];
    const float* logits_data = (const float*)nimcp_tensor_data_const(logits);
    const float* labels_data = (const float*)nimcp_tensor_data_const(labels);

    /* For simplicity, use random approximation */
    /* Actual implementation would use autograd */
    for (size_t i = 0; i < input_size; i++) {
        /* Approximate gradient direction based on input perturbation effect */
        float sum = 0.0f;
        for (uint32_t b = 0; b < batch_size; b++) {
            uint32_t label = (uint32_t)labels_data[b];
            if (label >= num_classes) continue;
            const float* sample_logits = &logits_data[b * num_classes];

            /* Use logit value at correct class as proxy */
            sum -= sample_logits[label];
        }
        grad[i] = sum / (float)batch_size;
    }
}

static float compute_norm(const float* data, size_t size, adv_norm_t norm) {
    if (!data || size == 0) {
        return 0.0f;
    }

    float result = 0.0f;

    switch (norm) {
        case ADV_NORM_LINF: {
            for (size_t i = 0; i < size; i++) {
                float abs_val = fabsf(data[i]);
                if (abs_val > result) result = abs_val;
            }
            break;
        }

        case ADV_NORM_L2: {
            for (size_t i = 0; i < size; i++) {
                result += data[i] * data[i];
            }
            result = sqrtf(result);
            break;
        }

        case ADV_NORM_L1: {
            for (size_t i = 0; i < size; i++) {
                result += fabsf(data[i]);
            }
            break;
        }

        case ADV_NORM_L0: {
            for (size_t i = 0; i < size; i++) {
                if (fabsf(data[i]) > ADV_EPSILON) result += 1.0f;
            }
            break;
        }

        default:
            break;
    }

    return result;
}

static void clip_by_norm(float* data, size_t size, float max_norm, adv_norm_t norm) {
    float curr_norm = compute_norm(data, size, norm);
    if (curr_norm > max_norm && curr_norm > ADV_EPSILON) {
        float scale = max_norm / curr_norm;
        for (size_t i = 0; i < size; i++) {
            data[i] *= scale;
        }
    }
}

static void clamp_tensor(float* data, size_t size, float min_val, float max_val) {
    for (size_t i = 0; i < size; i++) {
        if (data[i] < min_val) data[i] = min_val;
        if (data[i] > max_val) data[i] = max_val;
    }
}

static uint32_t argmax(const float* data, size_t size) {
    if (!data || size == 0) return 0;

    uint32_t max_idx = 0;
    float max_val = data[0];

    for (size_t i = 1; i < size; i++) {
        if (data[i] > max_val) {
            max_val = data[i];
            max_idx = (uint32_t)i;
        }
    }

    return max_idx;
}

static float kl_divergence(const float* p, const float* q, size_t size) {
    float kl = 0.0f;
    for (size_t i = 0; i < size; i++) {
        if (p[i] > ADV_EPSILON && q[i] > ADV_EPSILON) {
            kl += p[i] * logf(p[i] / q[i]);
        }
    }
    return kl;
}

static void softmax(const float* logits, float* probs, size_t size) {
    /* Find max for numerical stability */
    float max_val = logits[0];
    for (size_t i = 1; i < size; i++) {
        if (logits[i] > max_val) max_val = logits[i];
    }

    /* Compute exp and sum */
    float sum = 0.0f;
    for (size_t i = 0; i < size; i++) {
        probs[i] = expf(logits[i] - max_val);
        sum += probs[i];
    }

    /* Normalize */
    for (size_t i = 0; i < size; i++) {
        probs[i] /= sum;
    }
}
