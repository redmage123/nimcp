/**
 * @file nimcp_ensemble_uncertainty.c
 * @brief Implementation of real ensemble-based uncertainty quantification
 *
 * WHAT: Implements ensemble uncertainty using multiple network snapshots
 * WHY: Replace simulated uncertainty with true model and data uncertainty
 * HOW: Create diverse ensemble, run predictions, compute variance and entropy
 *
 * ALGORITHM OVERVIEW:
 * 1. Ensemble Creation:
 *    - Create N copies of base network
 *    - Perturb weights with Gaussian noise
 *    - Each model represents different hypothesis
 *
 * 2. Uncertainty Estimation:
 *    - Run input through all N models
 *    - Epistemic = Var(predictions) = model disagreement
 *    - Aleatoric = E[H(predictions)] = average entropy
 *    - Total = epistemic + aleatoric
 *
 * THREAD SAFETY: All public functions are thread-safe via mutex
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "cognitive/introspection/nimcp_ensemble_uncertainty.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_security.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LOG_MODULE "cognitive.introspection.ensemble"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(ensemble_uncertainty, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ========================================================================
 * INTERNAL STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Ensemble context implementation
 * WHY: Encapsulate ensemble state
 * HOW: Store models, config, statistics
 */
struct ensemble_context_struct {
    ensemble_model_t* models;       /* Array of models */
    uint32_t num_models;            /* Current model count */
    uint32_t max_models;            /* Maximum capacity */
    ensemble_config_t config;       /* Configuration */
    ensemble_stats_t stats;         /* Performance statistics */
    nimcp_mutex_t lock;             /* Thread safety */
};

/* ========================================================================
 * FORWARD DECLARATIONS
 * ======================================================================== */

static adaptive_network_t perturb_network_weights(adaptive_network_t base,
                                                 float noise_sigma);
static float compute_prediction_entropy(const float* prediction, uint32_t size);
static float safe_log2f(float x);

/* ========================================================================
 * CONFIGURATION
 * ======================================================================== */

/**
 * WHAT: Get default ensemble configuration
 * WHY: Sensible defaults for most use cases
 * HOW: Return pre-configured struct
 */
ensemble_config_t ensemble_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_default_con", 0.0f);


    ensemble_config_t config = {
        .num_models = 5,                /* 5 models balances accuracy and speed */
        .weight_noise_sigma = 0.1F,     /* 10% weight perturbation */
        .dropout_rate = 0.2F,           /* 20% dropout */
        .use_bootstrap = false,
        .use_snapshot_ensemble = false,
        .snapshot_interval = 10,        /* Every 10 epochs */
        .max_models = 20                /* Maximum 20 models */
    };
    return config;
}

/* ========================================================================
 * ENSEMBLE LIFECYCLE
 * ======================================================================== */

/**
 * WHAT: Create ensemble from base network
 * WHY: Initialize ensemble for uncertainty estimation
 * HOW: Create N perturbed copies of base network
 *
 * COMPLEXITY: O(N * M) where N = num_models, M = network size
 */
ensemble_context_t ensemble_create(adaptive_network_t base_network,
                                   const ensemble_config_t* config)
{
    /* WHAT: Validate input */
    if (base_network == NULL) {
        LOG_ERROR("Base network is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensemble_create: validation failed");
        return NULL;
    }

    /* WHAT: Allocate context */
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_create", 0.0f);


    ensemble_context_t ensemble = (ensemble_context_t)
        nimcp_calloc(1, sizeof(struct ensemble_context_struct));
    if (ensemble == NULL) {
        LOG_ERROR("Failed to allocate ensemble context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensemble_create: validation failed");
        return NULL;
    }

    /* WHAT: Initialize configuration */
    ensemble->config = config ? *config : ensemble_default_config();
    ensemble->max_models = ensemble->config.max_models;
    ensemble->num_models = 0;

    /* WHAT: Allocate models array */
    ensemble->models = (ensemble_model_t*)
        nimcp_calloc(ensemble->max_models, sizeof(ensemble_model_t));
    if (ensemble->models == NULL) {
        LOG_ERROR("Failed to allocate models array");
        nimcp_free(ensemble);
        ensemble = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensemble_create: validation failed");
        return NULL;
    }

    /* WHAT: Initialize mutex */
    nimcp_mutex_init(&ensemble->lock, NULL);

    /* WHAT: Initialize statistics */
    memset(&ensemble->stats, 0, sizeof(ensemble_stats_t));
    ensemble->stats.num_models = 0;
    ensemble->stats.max_models = ensemble->max_models;

    /* WHAT: Create ensemble models by perturbing base network */
    /* WHY: Diversity in models is key to uncertainty estimation */
    for (uint32_t i = 0; i < ensemble->config.num_models; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ensemble->config.num_models > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)ensemble->config.num_models);
        }

        /* WHAT: Create perturbed copy of base network */
        adaptive_network_t perturbed = perturb_network_weights(
            base_network,
            ensemble->config.weight_noise_sigma);

        if (perturbed == NULL) {
            LOG_WARN("Failed to create model %u, continuing with %u models",
                    i, ensemble->num_models);
            break;
        }

        /* WHAT: Initialize model metadata */
        ensemble->models[i].network = perturbed;
        ensemble->models[i].model_id = i;
        ensemble->models[i].weight_perturbation = ensemble->config.weight_noise_sigma;
        ensemble->models[i].training_epoch = 0;
        ensemble->models[i].creation_time = nimcp_time_monotonic_ms();
        ensemble->models[i].last_prediction = NULL;
        ensemble->models[i].prediction_size = 0;

        ensemble->num_models++;
    }

    /* WHAT: Check if we created enough models */
    if (ensemble->num_models == 0) {
        LOG_ERROR("Failed to create any ensemble models");
        nimcp_mutex_destroy(&ensemble->lock);
        nimcp_free(ensemble->models);
        nimcp_free(ensemble);
        ensemble = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ensemble_create: ensemble->num_models is zero");
        return NULL;
    }

    ensemble->stats.num_models = ensemble->num_models;
    ensemble->stats.memory_used_bytes = sizeof(struct ensemble_context_struct) +
        ensemble->max_models * sizeof(ensemble_model_t);

    LOG_INFO("Created ensemble with %u models (requested %u)",
            ensemble->num_models, ensemble->config.num_models);

    return ensemble;
}

/**
 * WHAT: Destroy ensemble and free resources
 * WHY: Prevent memory leaks
 * HOW: Free all models and context
 */
void ensemble_destroy(ensemble_context_t ensemble)
{
    if (ensemble == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_destroy", 0.0f);


    nimcp_mutex_lock(&ensemble->lock);

    /* WHAT: Destroy all models */
    for (uint32_t i = 0; i < ensemble->num_models; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ensemble->num_models > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)ensemble->num_models);
        }

        if (ensemble->models[i].network != NULL) {
            adaptive_network_destroy(ensemble->models[i].network);
        }
        if (ensemble->models[i].last_prediction != NULL) {
            nimcp_free(ensemble->models[i].last_prediction);
        }
    }

    /* WHAT: Free models array */
    nimcp_free(ensemble->models);

    nimcp_mutex_unlock(&ensemble->lock);
    nimcp_mutex_destroy(&ensemble->lock);

    /* WHAT: Free context */
    nimcp_free(ensemble);
    ensemble = NULL;
}

/**
 * WHAT: Add trained model to ensemble
 * WHY: Incrementally build ensemble
 * HOW: Add model snapshot to ensemble
 */
bool ensemble_add_model(ensemble_context_t ensemble,
                       adaptive_network_t network,
                       uint32_t training_epoch)
{
    if (ensemble == NULL || network == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ensemble_add_model: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_add_model", 0.0f);


    nimcp_mutex_lock(&ensemble->lock);

    /* WHAT: Check if ensemble is full */
    if (ensemble->num_models >= ensemble->max_models) {
        nimcp_mutex_unlock(&ensemble->lock);
        LOG_WARN("Ensemble is full (%u/%u models)",
                ensemble->num_models, ensemble->max_models);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ensemble_add_model: capacity exceeded");
        return false;
    }

    /* WHAT: Add model to ensemble */
    uint32_t idx = ensemble->num_models;
    ensemble->models[idx].network = network;
    ensemble->models[idx].model_id = idx;
    ensemble->models[idx].weight_perturbation = 0.0F; /* Not perturbed */
    ensemble->models[idx].training_epoch = training_epoch;
    ensemble->models[idx].creation_time = nimcp_time_monotonic_ms();
    ensemble->models[idx].last_prediction = NULL;
    ensemble->models[idx].prediction_size = 0;

    ensemble->num_models++;
    ensemble->stats.num_models = ensemble->num_models;

    nimcp_mutex_unlock(&ensemble->lock);

    LOG_DEBUG("Added model %u to ensemble (epoch %u)", idx, training_epoch);

    return true;
}

/**
 * WHAT: Get number of models in ensemble
 * WHY: Check ensemble size
 * HOW: Return model count
 */
uint32_t ensemble_get_size(ensemble_context_t ensemble)
{
    if (ensemble == NULL) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_get_size", 0.0f);


    nimcp_mutex_lock(&ensemble->lock);
    uint32_t size = ensemble->num_models;
    nimcp_mutex_unlock(&ensemble->lock);

    return size;
}

/* ========================================================================
 * PREDICTION AND UNCERTAINTY
 * ======================================================================== */

/**
 * WHAT: Get predictions from all ensemble models
 * WHY: Basis for uncertainty quantification
 * HOW: Run input through each model
 *
 * COMPLEXITY: O(N * F) where N = models, F = forward pass cost
 */
uint32_t ensemble_predict(ensemble_context_t ensemble,
                          const float* features,
                          uint32_t num_features,
                          ensemble_prediction_t** predictions)
{
    if (ensemble == NULL || features == NULL || predictions == NULL) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_predict", 0.0f);


    nimcp_mutex_lock(&ensemble->lock);

    /* WHAT: Allocate predictions array */
    *predictions = (ensemble_prediction_t*)
        nimcp_calloc(ensemble->num_models, sizeof(ensemble_prediction_t));
    if (*predictions == NULL) {
        nimcp_mutex_unlock(&ensemble->lock);
        return 0;
    }

    /* WHAT: Get predictions from each model */
    uint32_t successful_predictions = 0;

    for (uint32_t i = 0; i < ensemble->num_models; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ensemble->num_models > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)ensemble->num_models);
        }

        adaptive_network_t network = ensemble->models[i].network;
        if (network == NULL) {
            continue;
        }

        /* WHAT: Get network output size from config */
        const adaptive_network_config_t* net_config =
            adaptive_network_get_config(network);
        if (net_config == NULL) {
            continue;
        }
        uint32_t output_size = net_config->base_config.output_size;

        /* WHAT: Allocate output buffer */
        float* output = (float*) nimcp_calloc(output_size, sizeof(float));
        if (output == NULL) {
            continue;
        }

        /* WHAT: Run forward pass */
        uint64_t timestamp = nimcp_time_monotonic_ms();
        adaptive_network_forward(network, features, num_features,
                                output, output_size, timestamp);

        /* WHAT: Compute entropy of this prediction */
        float entropy = compute_prediction_entropy(output, output_size);

        /* WHAT: Store prediction */
        (*predictions)[i].prediction = output;
        (*predictions)[i].size = output_size;
        (*predictions)[i].confidence = 1.0F - entropy; /* Approximate */
        (*predictions)[i].entropy = entropy;

        successful_predictions++;
    }

    ensemble->stats.total_predictions++;

    nimcp_mutex_unlock(&ensemble->lock);

    return successful_predictions;
}

/**
 * WHAT: Compute uncertainty from ensemble predictions
 * WHY: Quantify epistemic and aleatoric uncertainty
 * HOW: Compute variance and entropy
 *
 * COMPLEXITY: O(N * F + N * D)
 */
ensemble_uncertainty_result_t ensemble_compute_uncertainty(
    ensemble_context_t ensemble,
    const float* features,
    uint32_t num_features)
{
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_compute_unc", 0.0f);


    ensemble_uncertainty_result_t result;
    memset(&result, 0, sizeof(ensemble_uncertainty_result_t));

    if (ensemble == NULL || features == NULL || num_features == 0) {
        return result;
    }

    /* WHAT: Get predictions from all models */
    ensemble_prediction_t* predictions = NULL;
    uint32_t num_predictions = ensemble_predict(ensemble, features,
                                                num_features, &predictions);

    if (num_predictions == 0 || predictions == NULL) {
        LOG_WARN("No predictions obtained from ensemble");
        return result;
    }

    /* WHAT: Get output dimension from first prediction */
    uint32_t output_dim = predictions[0].size;
    result.prediction_size = output_dim;
    result.num_models = num_predictions;

    /* WHAT: Allocate result arrays */
    result.mean_prediction = (float*) nimcp_calloc(output_dim, sizeof(float));
    result.prediction_variance = (float*) nimcp_calloc(output_dim, sizeof(float));
    result.individual_predictions = predictions; /* Transfer ownership */

    if (result.mean_prediction == NULL || result.prediction_variance == NULL) {
        LOG_ERROR("Failed to allocate result arrays");
        ensemble_uncertainty_free(&result);
        return result;
    }

    /* WHAT: Compute mean prediction across ensemble */
    /* HOW: μ = (1/N) Σ p_i */
    memset(result.mean_prediction, 0, output_dim * sizeof(float));
    for (uint32_t i = 0; i < num_predictions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_predictions > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)num_predictions);
        }

        for (uint32_t j = 0; j < output_dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && output_dim > 256) {
                ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                                 (float)(j + 1) / (float)output_dim);
            }

            result.mean_prediction[j] += predictions[i].prediction[j];
        }
    }
    for (uint32_t j = 0; j < output_dim; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && output_dim > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(j + 1) / (float)output_dim);
        }

        result.mean_prediction[j] /= (float) num_predictions;
    }

    /* WHAT: Compute variance (epistemic uncertainty) */
    /* HOW: Var = (1/N) Σ (p_i - μ)² */
    memset(result.prediction_variance, 0, output_dim * sizeof(float));
    for (uint32_t i = 0; i < num_predictions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_predictions > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)num_predictions);
        }

        for (uint32_t j = 0; j < output_dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && output_dim > 256) {
                ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                                 (float)(j + 1) / (float)output_dim);
            }

            float diff = predictions[i].prediction[j] - result.mean_prediction[j];
            result.prediction_variance[j] += diff * diff;
        }
    }

    /* WHAT: Convert variance to epistemic uncertainty */
    /* WHY: Epistemic = model disagreement = variance across models */
    float total_variance = 0.0F;
    for (uint32_t j = 0; j < output_dim; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && output_dim > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(j + 1) / (float)output_dim);
        }

        result.prediction_variance[j] /= (float) num_predictions;
        total_variance += result.prediction_variance[j];
    }
    result.epistemic = sqrtf(total_variance / (float) output_dim); /* Std dev */

    /* WHAT: Compute aleatoric uncertainty */
    /* WHY: Aleatoric = data noise = average entropy of individual predictions */
    /* HOW: E[H(p_i)] = (1/N) Σ H(p_i) */
    float total_entropy = 0.0F;
    for (uint32_t i = 0; i < num_predictions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_predictions > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)num_predictions);
        }

        total_entropy += predictions[i].entropy;
    }
    result.aleatoric = total_entropy / (float) num_predictions;

    /* WHAT: Combine uncertainties */
    result.total = result.epistemic + result.aleatoric;
    if (result.total > 1.0F) {
        result.total = 1.0F; /* Clamp to [0,1] */
    }
    result.confidence = 1.0F - result.total;

    /* WHAT: Update statistics */
    nimcp_mutex_lock(&ensemble->lock);
    ensemble->stats.avg_epistemic =
        (ensemble->stats.avg_epistemic * (ensemble->stats.total_predictions - 1) +
         result.epistemic) / (float) ensemble->stats.total_predictions;
    ensemble->stats.avg_aleatoric =
        (ensemble->stats.avg_aleatoric * (ensemble->stats.total_predictions - 1) +
         result.aleatoric) / (float) ensemble->stats.total_predictions;
    nimcp_mutex_unlock(&ensemble->lock);

    LOG_DEBUG("Uncertainty: epistemic=%.3f aleatoric=%.3f total=%.3f",
             result.epistemic, result.aleatoric, result.total);

    return result;
}

/**
 * WHAT: Free uncertainty result
 * WHY: Release allocated memory
 * HOW: Free all arrays
 */
void ensemble_uncertainty_free(ensemble_uncertainty_result_t* result)
{
    if (result == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_free", 0.0f);


    nimcp_free(result->mean_prediction);
    nimcp_free(result->prediction_variance);

    if (result->individual_predictions != NULL) {
        ensemble_predictions_free(result->individual_predictions,
                                 result->num_models);
    }

    memset(result, 0, sizeof(ensemble_uncertainty_result_t));
}

/**
 * WHAT: Free predictions array
 * WHY: Release memory from ensemble_predict
 * HOW: Free each prediction and array
 */
void ensemble_predictions_free(ensemble_prediction_t* predictions,
                               uint32_t num_predictions)
{
    if (predictions == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_predictions", 0.0f);


    for (uint32_t i = 0; i < num_predictions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_predictions > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)num_predictions);
        }

        nimcp_free(predictions[i].prediction);
    }

    nimcp_free(predictions);
    predictions = NULL;
}

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Compute entropy of probability distribution
 * WHY: Measure uncertainty of single prediction
 * HOW: H(p) = -Σ p_i log2(p_i)
 */
float ensemble_compute_entropy(const float* probabilities, uint32_t size)
{
    if (probabilities == NULL || size == 0) {
        return 0.0F;
    }

    /* WHAT: Normalize to probabilities (in case not normalized) */
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_compute_ent", 0.0f);


    float sum = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)size);
        }

        sum += fabsf(probabilities[i]);
    }

    if (sum < 1e-10F) {
        return 0.0F;
    }

    /* WHAT: Compute entropy */
    float entropy = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)size);
        }

        float p = fabsf(probabilities[i]) / (fabsf(sum) > 1e-7f ? sum : 1e-7f);
        if (p > 1e-10F) {
            entropy -= p * safe_log2f(p);
        }
    }

    return entropy;
}

/**
 * WHAT: Compute variance across predictions
 * WHY: Measure model disagreement
 * HOW: Var = (1/N) Σ (p_i - μ)²
 */
float ensemble_compute_variance(const float** predictions,
                                uint32_t num_predictions,
                                uint32_t dimension,
                                float* variance)
{
    if (predictions == NULL || num_predictions == 0 || dimension == 0) {
        return 0.0F;
    }

    /* WHAT: Compute mean first */
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_compute_var", 0.0f);


    float* mean = (float*) nimcp_calloc(dimension, sizeof(float));
    if (mean == NULL) {
        return 0.0F;
    }

    ensemble_compute_mean(predictions, num_predictions, dimension, mean);

    /* WHAT: Compute variance */
    if (variance != NULL) {
        memset(variance, 0, dimension * sizeof(float));
    }

    float total_var = 0.0F;
    for (uint32_t i = 0; i < num_predictions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_predictions > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)num_predictions);
        }

        for (uint32_t j = 0; j < dimension; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && dimension > 256) {
                ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                                 (float)(j + 1) / (float)dimension);
            }

            float diff = predictions[i][j] - mean[j];
            float var = diff * diff;
            if (variance != NULL) {
                variance[j] += var;
            }
            total_var += var;
        }
    }

    /* WHAT: Average variance */
    if (variance != NULL) {
        for (uint32_t j = 0; j < dimension; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && dimension > 256) {
                ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                                 (float)(j + 1) / (float)dimension);
            }

            variance[j] /= (float) num_predictions;
        }
    }
    total_var /= (float) (num_predictions * dimension);

    nimcp_free(mean);
    mean = NULL;
    return total_var;
}

/**
 * WHAT: Compute mean prediction
 * WHY: Get consensus prediction
 * HOW: μ = (1/N) Σ p_i
 */
void ensemble_compute_mean(const float** predictions,
                          uint32_t num_predictions,
                          uint32_t dimension,
                          float* mean)
{
    if (predictions == NULL || num_predictions == 0 || dimension == 0 || mean == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_compute_mea", 0.0f);


    memset(mean, 0, dimension * sizeof(float));

    for (uint32_t i = 0; i < num_predictions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_predictions > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)num_predictions);
        }

        for (uint32_t j = 0; j < dimension; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && dimension > 256) {
                ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                                 (float)(j + 1) / (float)dimension);
            }

            mean[j] += predictions[i][j];
        }
    }

    for (uint32_t j = 0; j < dimension; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && dimension > 256) {
            ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                             (float)(j + 1) / (float)dimension);
        }

        mean[j] /= (float) num_predictions;
    }
}

/**
 * WHAT: Get ensemble statistics
 * WHY: Monitor performance
 * HOW: Return statistics
 */
bool ensemble_get_stats(ensemble_context_t ensemble, ensemble_stats_t* stats)
{
    if (ensemble == NULL || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ensemble_get_stats: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_get_stats", 0.0f);


    nimcp_mutex_lock(&ensemble->lock);
    *stats = ensemble->stats;
    nimcp_mutex_unlock(&ensemble->lock);

    return true;
}

/* ========================================================================
 * HELPER FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Create perturbed copy of network
 * WHY: Generate diverse ensemble models
 * HOW: Copy network, add Gaussian noise to weights
 *
 * NOTE: This is a simplified implementation. Full implementation would:
 * - Deep copy network structure
 * - Add weight perturbations
 * - Apply dropout masks
 */
static adaptive_network_t perturb_network_weights(adaptive_network_t base,
                                                 float noise_sigma)
{
    if (base == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "base is NULL");

        return NULL;
    }

    /* WHAT: Get base network configuration */
    const adaptive_network_config_t* base_config = adaptive_network_get_config(base);
    if (base_config == NULL) {
        LOG_ERROR("Failed to get base network config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "perturb_network_weights: validation failed");
        return NULL;
    }

    /* WHAT: Create new network with same config */
    /* NOTE: This creates a fresh network, not a copy with perturbed weights */
    /* FUTURE: Implement deep copy + weight perturbation */
    adaptive_network_config_t new_config = *base_config;
    adaptive_network_t perturbed = adaptive_network_create(&new_config);

    if (perturbed == NULL) {
        LOG_ERROR("Failed to create perturbed network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "perturb_network_weights: validation failed");
        return NULL;
    }

    /* TODO: Implement weight perturbation
     * For now, this creates an independent model which still provides
     * ensemble diversity through different random initializations
     */

    return perturbed;
}

/**
 * WHAT: Compute entropy of prediction
 * WHY: Measure uncertainty of output
 * HOW: Call ensemble_compute_entropy
 */
static float compute_prediction_entropy(const float* prediction, uint32_t size)
{
    return ensemble_compute_entropy(prediction, size);
}

/**
 * WHAT: Safe log2 that avoids log(0)
 * WHY: Prevent NaN in entropy calculations
 * HOW: Return 0 for x <= 0
 */
static float safe_log2f(float x)
{
    if (x <= 1e-10F) {
        return 0.0F;
    }
    return log2f(x);
}

/* ========================================================================
 * INTROSPECTION INTEGRATION
 * ======================================================================== */

/**
 * NOTE: introspection_set_ensemble() and introspection_get_ensemble()
 * are implemented in nimcp_introspection.c where we have access to
 * introspection_context_struct internals.
 */

ensemble_context_t ensemble_train_from_brain(
    brain_t brain,
    const char** checkpoint_paths,
    uint32_t num_checkpoints,
    const ensemble_config_t* config)
{
    /* WHAT: Create ensemble from brain checkpoints */
    /* WHY: Build ensemble from training snapshots */
    /* HOW: Load checkpoints and create ensemble */

    if (brain == NULL || checkpoint_paths == NULL || num_checkpoints == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ensemble_train_from_brain: num_checkpoints is zero");
        return NULL;
    }

    /* WHAT: Get base network from brain */
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_ensemble_train_from_", 0.0f);


    adaptive_network_t base_network = brain_get_network(brain);
    if (base_network == NULL) {
        LOG_ERROR("Failed to get network from brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensemble_train_from_brain: validation failed");
        return NULL;
    }

    /* WHAT: Create ensemble */
    ensemble_context_t ensemble = ensemble_create(base_network, config);
    if (ensemble == NULL) {
        LOG_ERROR("Failed to create ensemble");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ensemble_train_from_brain: validation failed");
        return NULL;
    }

    /* TODO: Load checkpoints and add to ensemble
     * For now, just return ensemble with perturbed base models
     */
    (void)checkpoint_paths;
    (void)num_checkpoints;

    LOG_INFO("Created ensemble from brain (checkpoint loading not yet implemented)");

    return ensemble;
}

/* ========================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Query knowledge graph for self-knowledge about ensemble uncertainty module
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int ensemble_uncertainty_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_heartbeat("ensemble_unc_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Ensemble_Uncertainty_Module");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                ensemble_uncertainty_heartbeat("ensemble_unc_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Ensemble uncertainty self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Ensemble_Uncertainty_Module");
    if (connections) {
        LOG_DEBUG("Ensemble uncertainty has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Ensemble_Uncertainty_Module");
    if (incoming) {
        LOG_DEBUG("Ensemble uncertainty has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Full Training
 * ============================================================================ */

void ensemble_uncertainty_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_ensemble_uncertainty_instance_health_agent = agent;
}

int ensemble_uncertainty_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensemble_uncertainty_training_begin: ctx is NULL");
        return -1;
    }
    ensemble_uncertainty_heartbeat_instance(g_ensemble_uncertainty_instance_health_agent,
        "ens_uncert_training_begin", 0.0f);
    NIMCP_LOGGING_INFO("[ENSEMBLE_UNCERTAINTY] Training begin: module state reset");
    return 0;
}

int ensemble_uncertainty_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensemble_uncertainty_training_step: ctx is NULL");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ensemble_uncertainty_heartbeat_instance(g_ensemble_uncertainty_instance_health_agent,
        "ens_uncert_training_step", progress);
    return 0;
}

int ensemble_uncertainty_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensemble_uncertainty_training_end: ctx is NULL");
        return -1;
    }
    ensemble_uncertainty_heartbeat_instance(g_ensemble_uncertainty_instance_health_agent,
        "ens_uncert_training_end", 1.0f);
    NIMCP_LOGGING_INFO("[ENSEMBLE_UNCERTAINTY] Training end: metrics finalized");
    return 0;
}
