/**
 * @file nimcp_learning_signal_adapter.c
 * @brief Learning Signal Adapter Implementation
 *
 * WHAT: Connects middleware features to learning signal computation
 * WHY:  Enable learning from neural activity patterns
 * HOW:  Extract features, compute prediction errors, generate learning signals
 *
 * @author NIMCP Development Team
 * @date 2026-03-12
 */

#include "middleware/training/nimcp_learning_signal_adapter.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "learning_signal_adapter"

/* ============================================================================
 * Default Constants
 * ============================================================================ */

#define DEFAULT_NUM_CHANNELS        64
#define DEFAULT_BUFFER_SIZE         256
#define DEFAULT_LEARNING_RATE       0.01f
#define DEFAULT_DECAY_RATE          0.95f
#define DEFAULT_HISTORY_SIZE        256

/* Weights for combined signal */
#define WEIGHT_PREDICTION_ERROR     0.4f
#define WEIGHT_REWARD               0.3f
#define WEIGHT_NOVELTY              0.2f
#define WEIGHT_SYNCHRONY            0.1f

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct learning_signal_adapter_struct {
    learning_signal_adapter_config_t config;
    float* eligibility_traces;      /**< [num_channels] */
    float* prediction_history;      /**< Ring buffer for novelty detection */
    uint32_t history_pos;
    uint32_t history_count;
    uint32_t history_size;
    learning_signal_t last_signal;
};

/* ============================================================================
 * Helper: Resolve buffer size to history capacity
 * ============================================================================ */

static uint32_t resolve_history_size(brain_buffer_size_t bsz)
{
    switch (bsz) {
        case BUFFER_SIZE_10MS:   return 64;
        case BUFFER_SIZE_100MS:  return 256;
        case BUFFER_SIZE_1S:     return 1024;
        case BUFFER_SIZE_CUSTOM: return DEFAULT_HISTORY_SIZE;
        default:                 return DEFAULT_HISTORY_SIZE;
    }
}

/* ============================================================================
 * learning_signal_adapter_default_config
 * ============================================================================ */

learning_signal_adapter_config_t learning_signal_adapter_default_config(void)
{
    learning_signal_adapter_config_t config;
    memset(&config, 0, sizeof(config));

    config.num_channels            = DEFAULT_NUM_CHANNELS;
    config.buffer_size             = BUFFER_SIZE_100MS;
    config.signal_type             = LEARNING_SIGNAL_COMBINED;
    config.learning_rate           = DEFAULT_LEARNING_RATE;
    config.decay_rate              = DEFAULT_DECAY_RATE;
    config.enable_eligibility_traces = true;
    config.enable_modulation       = true;

    return config;
}

/* ============================================================================
 * learning_signal_adapter_create
 * ============================================================================ */

learning_signal_adapter_t learning_signal_adapter_create(
    const learning_signal_adapter_config_t* config)
{
    if (!config) {
        LOG_ERROR("[%s] NULL config provided", LOG_MODULE);
        return NULL;
    }

    if (config->num_channels == 0) {
        LOG_ERROR("[%s] num_channels must be > 0", LOG_MODULE);
        return NULL;
    }

    struct learning_signal_adapter_struct* adapter =
        (struct learning_signal_adapter_struct*)nimcp_calloc(
            1, sizeof(struct learning_signal_adapter_struct));
    if (!adapter) {
        LOG_ERROR("[%s] Failed to allocate adapter", LOG_MODULE);
        return NULL;
    }

    adapter->config = *config;
    adapter->history_size = resolve_history_size(config->buffer_size);
    adapter->history_pos = 0;
    adapter->history_count = 0;
    memset(&adapter->last_signal, 0, sizeof(learning_signal_t));

    /* Allocate eligibility traces */
    adapter->eligibility_traces =
        (float*)nimcp_calloc(config->num_channels, sizeof(float));
    if (!adapter->eligibility_traces) {
        LOG_ERROR("[%s] Failed to allocate eligibility traces", LOG_MODULE);
        nimcp_free(adapter);
        return NULL;
    }

    /* Allocate prediction history ring buffer (stores per-step MSE values) */
    adapter->prediction_history =
        (float*)nimcp_calloc(adapter->history_size, sizeof(float));
    if (!adapter->prediction_history) {
        LOG_ERROR("[%s] Failed to allocate prediction history", LOG_MODULE);
        nimcp_free(adapter->eligibility_traces);
        nimcp_free(adapter);
        return NULL;
    }

    LOG_INFO("[%s] Created adapter: channels=%u, history_size=%u, lr=%.4f, decay=%.4f",
             LOG_MODULE, config->num_channels, adapter->history_size,
             config->learning_rate, config->decay_rate);

    return adapter;
}

/* ============================================================================
 * learning_signal_adapter_destroy
 * ============================================================================ */

void learning_signal_adapter_destroy(learning_signal_adapter_t adapter)
{
    if (!adapter) {
        return;
    }

    if (adapter->eligibility_traces) {
        nimcp_free(adapter->eligibility_traces);
    }
    if (adapter->prediction_history) {
        nimcp_free(adapter->prediction_history);
    }

    nimcp_free(adapter);
}

/* ============================================================================
 * learning_signal_adapter_compute
 * ============================================================================ */

bool learning_signal_adapter_compute(
    learning_signal_adapter_t adapter,
    const float* activity_predicted,
    const float* activity_actual,
    uint32_t num_channels,
    uint64_t timestamp,
    learning_signal_t* signal_out)
{
    if (!adapter || !activity_predicted || !activity_actual || !signal_out) {
        LOG_ERROR("[%s] NULL argument to compute", LOG_MODULE);
        return false;
    }

    /* Use the minimum of provided channels and configured channels */
    uint32_t channels = (num_channels < adapter->config.num_channels)
                        ? num_channels : adapter->config.num_channels;

    if (channels == 0) {
        LOG_WARN("[%s] Zero channels, nothing to compute", LOG_MODULE);
        return false;
    }

    /* --- Prediction Error (MSE) --- */
    float mse = 0.0f;
    for (uint32_t i = 0; i < channels; i++) {
        float diff = activity_predicted[i] - activity_actual[i];
        mse += diff * diff;
    }
    mse /= (float)channels;

    /* --- Reward signal: mean positive prediction error (better than expected) ---
     * When actual > predicted, the system performed better than expected. */
    float reward_sum = 0.0f;
    uint32_t reward_count = 0;
    for (uint32_t i = 0; i < channels; i++) {
        float diff = activity_actual[i] - activity_predicted[i];
        if (diff > 0.0f) {
            reward_sum += diff;
            reward_count++;
        }
    }
    float reward = (reward_count > 0) ? (reward_sum / (float)reward_count) : 0.0f;

    /* --- Store MSE in prediction history ring buffer for novelty --- */
    adapter->prediction_history[adapter->history_pos] = mse;
    adapter->history_pos = (adapter->history_pos + 1) % adapter->history_size;
    if (adapter->history_count < adapter->history_size) {
        adapter->history_count++;
    }

    /* --- Novelty: variance of recent prediction errors --- */
    float novelty = 0.0f;
    if (adapter->history_count > 1) {
        float mean_err = 0.0f;
        for (uint32_t i = 0; i < adapter->history_count; i++) {
            mean_err += adapter->prediction_history[i];
        }
        mean_err /= (float)adapter->history_count;

        float var = 0.0f;
        for (uint32_t i = 0; i < adapter->history_count; i++) {
            float d = adapter->prediction_history[i] - mean_err;
            var += d * d;
        }
        novelty = var / (float)(adapter->history_count - 1);
    }

    /* --- Synchrony: mean absolute correlation between adjacent channels --- */
    float synchrony = 0.0f;
    if (channels > 1) {
        /* Compute mean of actual for normalization */
        float mean_actual = 0.0f;
        for (uint32_t i = 0; i < channels; i++) {
            mean_actual += activity_actual[i];
        }
        mean_actual /= (float)channels;

        /* Mean absolute deviation of adjacent channel pairs as a synchrony proxy */
        float corr_sum = 0.0f;
        for (uint32_t i = 0; i < channels - 1; i++) {
            float a = activity_actual[i] - mean_actual;
            float b = activity_actual[i + 1] - mean_actual;
            float denom = sqrtf((a * a + 1e-8f) * (b * b + 1e-8f));
            corr_sum += fabsf(a * b / denom);
        }
        synchrony = corr_sum / (float)(channels - 1);
    }

    /* --- Combined signal: weighted sum --- */
    float combined = WEIGHT_PREDICTION_ERROR * mse
                   + WEIGHT_REWARD * reward
                   + WEIGHT_NOVELTY * novelty
                   + WEIGHT_SYNCHRONY * synchrony;

    /* Apply learning rate scaling */
    combined *= adapter->config.learning_rate;

    /* --- Eligibility trace modulation --- */
    float eligibility = 1.0f;
    if (adapter->config.enable_eligibility_traces) {
        /* Mean eligibility across channels */
        float elig_sum = 0.0f;
        for (uint32_t i = 0; i < channels; i++) {
            elig_sum += adapter->eligibility_traces[i];
        }
        eligibility = elig_sum / (float)channels;

        if (adapter->config.enable_modulation) {
            combined *= (1.0f + eligibility);
        }
    }

    /* --- Fill output --- */
    signal_out->prediction_error = mse;
    signal_out->reward_signal    = reward;
    signal_out->novelty_signal   = novelty;
    signal_out->synchrony_signal = synchrony;
    signal_out->combined_signal  = combined;
    signal_out->eligibility      = eligibility;
    signal_out->timestamp        = timestamp;

    adapter->last_signal = *signal_out;

    return true;
}

/* ============================================================================
 * learning_signal_adapter_update_eligibility
 * ============================================================================ */

bool learning_signal_adapter_update_eligibility(
    learning_signal_adapter_t adapter,
    const float* activity,
    uint32_t num_channels,
    float dt)
{
    if (!adapter || !activity) {
        LOG_ERROR("[%s] NULL argument to update_eligibility", LOG_MODULE);
        return false;
    }

    if (!adapter->config.enable_eligibility_traces) {
        return true;  /* Nothing to do, but not an error */
    }

    uint32_t channels = (num_channels < adapter->config.num_channels)
                        ? num_channels : adapter->config.num_channels;

    float decay_factor = 1.0f - adapter->config.decay_rate * dt;
    if (decay_factor < 0.0f) {
        decay_factor = 0.0f;
    }

    for (uint32_t i = 0; i < channels; i++) {
        adapter->eligibility_traces[i] =
            adapter->eligibility_traces[i] * decay_factor + activity[i] * dt;
    }

    return true;
}
