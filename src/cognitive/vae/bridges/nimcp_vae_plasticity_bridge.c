/**
 * @file nimcp_vae_plasticity_bridge.c
 * @brief VAE-Plasticity Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Implements VAE-modulated neural plasticity and learning signals.
 *
 * BIO_MODULE: 0x1F1C
 */

#include "cognitive/vae/bridges/nimcp_vae_plasticity_bridge.h"
#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/nimcp_vae_latent.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define VAE_PLAST_MODULE_ID           BIO_MODULE_VAE_PLASTICITY_BRIDGE
#define VAE_PLAST_HISTORY_SIZE        64
#define VAE_PLAST_REPLAY_BUFFER_SIZE  1024
#define VAE_PLAST_EMA_ALPHA           0.95f
#define VAE_PLAST_COLLAPSE_KL_THRESH  0.1f

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static inline float randf(void)
{
    return (float)nimcp_tl_rand() / (float)RAND_MAX;
}

/**
 * @brief Update signal history for smoothing
 */
static void update_signal_history(vae_plasticity_bridge_t* bridge,
                                   float recon_error, float kl, float precision)
{
    if (!bridge) return;

    uint32_t idx = bridge->history_head;

    if (bridge->recon_error_history) {
        bridge->recon_error_history[idx] = recon_error;
    }
    if (bridge->kl_history) {
        bridge->kl_history[idx] = kl;
    }
    if (bridge->precision_history) {
        bridge->precision_history[idx] = precision;
    }

    bridge->history_head = (bridge->history_head + 1) % bridge->history_length;
}

/**
 * @brief Get smoothed signal value
 */
static float get_smoothed_signal(const float* history, uint32_t length)
{
    if (!history || length == 0) return 0.0f;

    float sum = 0.0f;
    for (uint32_t i = 0; i < length; i++) {
        sum += history[i];
    }
    return sum / length;
}

/**
 * @brief Compute novelty score from latent change
 */
static float compute_novelty(vae_plasticity_bridge_t* bridge,
                              const float* latent, uint32_t dim)
{
    if (!bridge || !latent || !bridge->latent_buffer) return 0.0f;

    float diff_sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = latent[i] - bridge->latent_buffer[i];
        diff_sum += diff * diff;
    }

    /* Update buffer for next comparison */
    memcpy(bridge->latent_buffer, latent, dim * sizeof(float));

    return sqrtf(diff_sum / dim);
}

/**
 * @brief Apply signal to modulation based on mapping config
 */
static float apply_mapping(const vae_plasticity_mapping_t* mapping, float signal)
{
    if (!mapping) return 0.0f;

    float modulated = signal;

    /* Invert if needed */
    if (mapping->invert) {
        modulated = 1.0f - modulated;
    }

    /* Apply scaling and offset */
    switch (mapping->mode) {
        case VAE_PLAST_MOD_MULTIPLICATIVE:
            modulated = 1.0f + (modulated - 0.5f) * 2.0f * mapping->scale;
            break;
        case VAE_PLAST_MOD_ADDITIVE:
            modulated = mapping->offset + modulated * mapping->scale;
            break;
        case VAE_PLAST_MOD_GATING:
            modulated = (modulated > 0.5f) ? 1.0f : 0.0f;
            break;
        case VAE_PLAST_MOD_THRESHOLD:
            modulated = modulated * mapping->scale;
            break;
    }

    return nimcp_clampf(modulated, mapping->min_value, mapping->max_value);
}

/**
 * @brief Update statistics
 */
static void update_stats(vae_plasticity_bridge_t* bridge,
                          float recon_error, float kl)
{
    if (!bridge) return;

    bridge->stats.total_updates++;
    bridge->stats.avg_reconstruction_error =
        VAE_PLAST_EMA_ALPHA * bridge->stats.avg_reconstruction_error +
        (1.0f - VAE_PLAST_EMA_ALPHA) * recon_error;
    bridge->stats.avg_kl_divergence =
        VAE_PLAST_EMA_ALPHA * bridge->stats.avg_kl_divergence +
        (1.0f - VAE_PLAST_EMA_ALPHA) * kl;

    if (kl < bridge->stats.min_kl_observed) {
        bridge->stats.min_kl_observed = kl;
    }
    if (kl > bridge->stats.max_kl_observed) {
        bridge->stats.max_kl_observed = kl;
    }

    bridge->stats.last_update_us = get_timestamp_us();
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_plasticity_bridge_default_config(vae_plasticity_bridge_config_t* config)
{
    if (!config) return NIMCP_ERROR_VAE_PLAST_NULL;

    memset(config, 0, sizeof(*config));

    /* STDP config */
    config->stdp_config.modulate_a_plus = true;
    config->stdp_config.modulate_a_minus = true;
    config->stdp_config.modulate_tau = false;
    config->stdp_config.recon_error_gain = 0.5f;
    config->stdp_config.precision_gain = 0.3f;
    config->stdp_config.novelty_threshold = 0.2f;

    /* BCM config */
    config->bcm_config.modulate_threshold = true;
    config->bcm_config.modulate_rate = false;
    config->bcm_config.kl_divergence_gain = 0.5f;
    config->bcm_config.target_activity = 0.1f;

    /* Homeostatic config */
    config->homeo_config.enable_scaling = true;
    config->homeo_config.enable_intrinsic = false;
    config->homeo_config.collapse_threshold = VAE_PLAST_COLLAPSE_KL_THRESH;
    config->homeo_config.scaling_rate = 0.01f;

    /* Consolidation config */
    config->consolidation.enable_replay = true;
    config->consolidation.replay_batch_size = NIMCP_DEFAULT_BATCH_SIZE;
    config->consolidation.replay_temperature = NIMCP_TEMPERATURE_DEFAULT;
    config->consolidation.consolidation_rate = 0.1f;
    config->consolidation.prioritized_replay = true;

    /* Global parameters */
    config->global_learning_rate = 0.001f;
    config->modulation_smoothing = 0.9f;
    config->min_learning_rate = 0.0001f;
    config->max_learning_rate = 0.1f;

    /* Update timing */
    config->update_interval_ms = 10.0f;
    config->continuous_modulation = true;

    config->enable_logging = false;

    return 0;
}

vae_plasticity_bridge_t* vae_plasticity_bridge_create(const vae_plasticity_bridge_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_plasticity_bridge_create: config is NULL");
        return NULL;
    }

    vae_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_plasticity_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_plasticity_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = VAE_PLAST_STATE_DISCONNECTED;
    bridge->is_initialized = false;
    bridge->creation_time_us = get_timestamp_us();

    /* Allocate signal history */
    bridge->history_length = VAE_PLAST_HISTORY_SIZE;
    bridge->recon_error_history = nimcp_calloc(VAE_PLAST_HISTORY_SIZE, sizeof(float));
    bridge->kl_history = nimcp_calloc(VAE_PLAST_HISTORY_SIZE, sizeof(float));
    if (!bridge->kl_history) return NULL;
    bridge->precision_history = nimcp_calloc(VAE_PLAST_HISTORY_SIZE, sizeof(float));

    if (!bridge->recon_error_history || !bridge->kl_history || !bridge->precision_history) {
        vae_plasticity_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_plasticity_bridge_create: required parameter is NULL (bridge->recon_error_history, bridge->kl_history, bridge->precision_history)");
        return NULL;
    }

    /* Initialize modulation state */
    bridge->current_modulation.learning_rate_mod = 1.0f;
    bridge->current_modulation.threshold_mod = 1.0f;
    bridge->current_modulation.scaling_factor = 1.0f;
    bridge->current_modulation.plasticity_enabled = true;

    /* Initialize statistics */
    bridge->stats.min_kl_observed = 1e6f;
    bridge->stats.max_kl_observed = 0.0f;
    bridge->stats.creation_time_us = bridge->creation_time_us;

    bridge->is_initialized = true;

    if (config->enable_logging) {
        nimcp_log_info(VAE_PLAST_MODULE_ID, "VAE-Plasticity Bridge created");
    }

    return bridge;
}

void vae_plasticity_bridge_destroy(vae_plasticity_bridge_t* bridge)
{
    if (!bridge) return;

    vae_plasticity_bridge_disconnect(bridge);

    nimcp_free(bridge->recon_error_history);
    nimcp_free(bridge->kl_history);
    nimcp_free(bridge->precision_history);
    nimcp_free(bridge->replay_buffer);
    nimcp_free(bridge->latent_buffer);
    nimcp_free(bridge->signal_buffer);

    /* Free mappings if allocated */
    nimcp_free(bridge->config.mappings);

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_plasticity_bridge_connect_vae(vae_plasticity_bridge_t* bridge, vae_system_t* vae)
{
    if (!bridge) return NIMCP_ERROR_VAE_PLAST_NULL;
    if (!vae) return NIMCP_ERROR_VAE_PLAST_NULL;

    bridge->vae = vae;

    /* Allocate latent buffer based on VAE dimension */
    uint32_t latent_dim = vae_get_latent_dim(vae);
    if (latent_dim > 0) {
        nimcp_free(bridge->latent_buffer);
        bridge->latent_buffer = nimcp_calloc(latent_dim, sizeof(float));
        if (!bridge->latent_buffer) return NIMCP_ERROR_VAE_PLAST_NO_MEMORY;

        nimcp_free(bridge->signal_buffer);
        bridge->signal_buffer = nimcp_calloc(latent_dim, sizeof(float));
        if (!bridge->signal_buffer) return NIMCP_ERROR_VAE_PLAST_NO_MEMORY;
    }

    /* Allocate replay buffer */
    if (bridge->config.consolidation.enable_replay) {
        uint32_t input_dim = vae_get_input_dim(bridge->vae);
        bridge->replay_buffer_size = VAE_PLAST_REPLAY_BUFFER_SIZE * input_dim;
        bridge->replay_buffer = nimcp_calloc(bridge->replay_buffer_size, sizeof(float));
        if (!bridge->replay_buffer) return -1;
    }

    if (bridge->plasticity_coordinator) {
        bridge->state = VAE_PLAST_STATE_CONNECTED;
    }

    if (bridge->config.enable_logging) {
        nimcp_log_info(VAE_PLAST_MODULE_ID, "VAE connected (latent_dim=%u)", latent_dim);
    }

    return 0;
}

int vae_plasticity_bridge_connect_plasticity(vae_plasticity_bridge_t* bridge, void* coordinator)
{
    if (!bridge) return NIMCP_ERROR_VAE_PLAST_NULL;

    bridge->plasticity_coordinator = coordinator;

    if (bridge->vae) {
        bridge->state = VAE_PLAST_STATE_CONNECTED;
    }

    if (bridge->config.enable_logging) {
        nimcp_log_info(VAE_PLAST_MODULE_ID, "Plasticity coordinator connected");
    }

    return 0;
}

int vae_plasticity_bridge_disconnect(vae_plasticity_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_PLAST_NULL;

    bridge->vae = NULL;
    bridge->plasticity_coordinator = NULL;
    bridge->state = VAE_PLAST_STATE_DISCONNECTED;

    return 0;
}

bool vae_plasticity_bridge_is_connected(const vae_plasticity_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_plasticity_bridge_is_connected: bridge is NULL");
        return false;
    }
    return bridge->state == VAE_PLAST_STATE_CONNECTED;
}

/* ============================================================================
 * Modulation API
 * ============================================================================ */

int vae_plasticity_update_modulation(vae_plasticity_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_PLAST_NULL;
    if (bridge->state != VAE_PLAST_STATE_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_PLAST_NOT_CONNECTED, "vae_plasticity_bridge: error condition");
        return NIMCP_ERROR_VAE_PLAST_NOT_CONNECTED;
    }

    bridge->state = VAE_PLAST_STATE_MODULATING;

    /* Get current VAE metrics */
    float recon_error = vae_plasticity_get_recon_error(bridge);
    float kl_div = vae_plasticity_get_kl_divergence(bridge);
    float precision = vae_plasticity_get_precision(bridge);
    float novelty = bridge->current_modulation.novelty_score;

    /* Update signal history */
    update_signal_history(bridge, recon_error, kl_div, precision);

    /* Apply smoothing */
    float smooth = bridge->config.modulation_smoothing;
    recon_error = smooth * bridge->current_modulation.recon_error +
                  (1.0f - smooth) * recon_error;
    kl_div = smooth * bridge->current_modulation.kl_divergence +
             (1.0f - smooth) * kl_div;
    precision = smooth * bridge->current_modulation.precision_avg +
                (1.0f - smooth) * precision;

    /* Update modulation state */
    bridge->current_modulation.recon_error = recon_error;
    bridge->current_modulation.kl_divergence = kl_div;
    bridge->current_modulation.precision_avg = precision;
    bridge->current_modulation.novelty_score = novelty;

    /* Compute learning rate modulation */
    float lr_mod = 1.0f;

    /* Higher reconstruction error → higher learning rate (prediction error) */
    lr_mod *= (1.0f + recon_error * bridge->config.stdp_config.recon_error_gain);

    /* Higher precision → higher confidence → adjust learning */
    lr_mod *= (1.0f + (precision - 1.0f) * bridge->config.stdp_config.precision_gain);

    /* Novelty boost */
    if (novelty > bridge->config.stdp_config.novelty_threshold) {
        lr_mod *= (1.0f + novelty * 0.5f);
    }

    /* Apply limits */
    lr_mod = nimcp_clampf(lr_mod,
                    bridge->config.min_learning_rate / bridge->config.global_learning_rate,
                    bridge->config.max_learning_rate / bridge->config.global_learning_rate);

    bridge->current_modulation.learning_rate_mod = lr_mod;

    /* Compute BCM threshold modulation */
    float thresh_mod = 1.0f + kl_div * bridge->config.bcm_config.kl_divergence_gain;
    bridge->current_modulation.threshold_mod = thresh_mod;

    /* Check for posterior collapse */
    bool collapse = vae_plasticity_detect_collapse(bridge);
    if (collapse) {
        bridge->stats.collapse_events++;
        /* Enable homeostatic scaling during collapse */
        bridge->current_modulation.scaling_factor = 1.0f +
            bridge->config.homeo_config.scaling_rate;
    } else {
        bridge->current_modulation.scaling_factor = 1.0f;
    }

    bridge->current_modulation.last_update_us = get_timestamp_us();

    /* Apply custom mappings */
    for (uint32_t m = 0; m < bridge->config.num_mappings; m++) {
        const vae_plasticity_mapping_t* mapping = &bridge->config.mappings[m];
        float signal = 0.0f;

        switch (mapping->signal) {
            case VAE_PLAST_SIG_RECON_ERROR:
                signal = recon_error;
                break;
            case VAE_PLAST_SIG_KL_DIVERGENCE:
                signal = kl_div;
                break;
            case VAE_PLAST_SIG_PRECISION:
                signal = precision;
                break;
            case VAE_PLAST_SIG_NOVELTY:
                signal = novelty;
                break;
            default:
                continue;
        }

        apply_mapping(mapping, signal);
    }

    update_stats(bridge, recon_error, kl_div);

    bridge->state = VAE_PLAST_STATE_CONNECTED;
    return 0;
}

int vae_plasticity_compute_signals(vae_plasticity_bridge_t* bridge,
                                    const float* input, uint32_t input_dim)
{
    if (!bridge || !input) return NIMCP_ERROR_VAE_PLAST_NULL;
    if (!bridge->vae) return NIMCP_ERROR_VAE_PLAST_NOT_CONNECTED;

    /* Encode input and compute metrics using tensor API */
    uint32_t latent_dim = vae_get_latent_dim(bridge->vae);
    nimcp_tensor_t* input_tensor = nimcp_tensor_create(&input_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* log_var_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !log_var_tensor) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_PLAST_NO_MEMORY, "vae_plasticity_bridge: error condition");
        return NIMCP_ERROR_VAE_PLAST_NO_MEMORY;
    }

    memcpy(TENSOR_DATA_F32(input_tensor), input, input_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, log_var_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_PLAST_UPDATE_FAILED, "vae_plasticity_bridge: error condition");
        return NIMCP_ERROR_VAE_PLAST_UPDATE_FAILED;
    }

    /* Compute novelty from latent change */
    float novelty = compute_novelty(bridge, TENSOR_DATA_F32(mu_tensor), latent_dim);
    bridge->current_modulation.novelty_score = novelty;

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(log_var_tensor);

    /* Update full modulation state */
    return vae_plasticity_update_modulation(bridge);
}

int vae_plasticity_apply_modulation(vae_plasticity_bridge_t* bridge,
                                     vae_plasticity_mechanism_t mechanism)
{
    if (!bridge) return NIMCP_ERROR_VAE_PLAST_NULL;

    /* This would integrate with actual plasticity coordinator */
    /* For now, just log the modulation application */

    if (bridge->config.enable_logging) {
        LOG_DEBUG("Applying modulation to %s: lr_mod=%.3f, thresh_mod=%.3f",
                  vae_plasticity_mechanism_to_string(mechanism),
                  bridge->current_modulation.learning_rate_mod,
                  bridge->current_modulation.threshold_mod);
    }

    return 0;
}

int vae_plasticity_get_modulation(const vae_plasticity_bridge_t* bridge,
                                   vae_plasticity_modulation_state_t* state)
{
    if (!bridge || !state) return NIMCP_ERROR_VAE_PLAST_NULL;
    *state = bridge->current_modulation;
    return 0;
}

/* ============================================================================
 * Signal Extraction API
 * ============================================================================ */

float vae_plasticity_get_recon_error(const vae_plasticity_bridge_t* bridge)
{
    if (!bridge || !bridge->vae) return 0.0f;

    vae_stats_t stats;
    if (vae_get_stats(bridge->vae, &stats) == 0) {
        return stats.ema_reconstruction_loss;
    }
    return 0.0f;
}

float vae_plasticity_get_kl_divergence(const vae_plasticity_bridge_t* bridge)
{
    if (!bridge || !bridge->vae) return 0.0f;

    vae_stats_t stats;
    if (vae_get_stats(bridge->vae, &stats) == 0) {
        return stats.ema_kl_divergence;
    }
    return 0.0f;
}

float vae_plasticity_get_precision(const vae_plasticity_bridge_t* bridge)
{
    if (!bridge || !bridge->vae) return 1.0f;

    /* Get average precision (1/variance) across latent dims */
    uint32_t dim = vae_get_latent_dim(bridge->vae);
    vae_latent_state_t* state = vae_latent_state_create(dim);
    if (!state) return 1.0f;

    int ret = vae_get_latent_state(bridge->vae, state);
    if (ret != 0 || !state->log_var) {
        vae_latent_state_destroy(state);
        return 1.0f;
    }

    float avg_precision = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float var = expf(state->log_var[i]);
        avg_precision += 1.0f / (var + 1e-6f);
    }

    vae_latent_state_destroy(state);
    return avg_precision / dim;
}

float vae_plasticity_get_novelty(const vae_plasticity_bridge_t* bridge)
{
    if (!bridge) return 0.0f;
    return bridge->current_modulation.novelty_score;
}

float vae_plasticity_get_elbo(const vae_plasticity_bridge_t* bridge)
{
    if (!bridge || !bridge->vae) return 0.0f;

    vae_stats_t stats;
    if (vae_get_stats(bridge->vae, &stats) == 0) {
        /* ELBO = -free_energy */
        return -stats.ema_free_energy;
    }
    return 0.0f;
}

bool vae_plasticity_detect_collapse(const vae_plasticity_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_plasticity_detect_collapse: bridge is NULL");
        return false;
    }

    float kl = vae_plasticity_get_kl_divergence(bridge);
    return kl < bridge->config.homeo_config.collapse_threshold;
}

/* ============================================================================
 * Consolidation API (VAE-based replay)
 * ============================================================================ */

int vae_plasticity_generate_replay(vae_plasticity_bridge_t* bridge,
                                    uint32_t num_samples,
                                    vae_plasticity_replay_result_t* result)
{
    if (!bridge || !result) return NIMCP_ERROR_VAE_PLAST_NULL;
    if (!bridge->vae) return NIMCP_ERROR_VAE_PLAST_NOT_CONNECTED;

    uint64_t start_us = get_timestamp_us();
    memset(result, 0, sizeof(*result));

    uint32_t output_dim = vae_get_input_dim(bridge->vae);
    uint32_t latent_dim = vae_get_latent_dim(bridge->vae);

    result->num_samples = num_samples;
    result->sample_dim = output_dim;
    result->generated_samples = nimcp_calloc(num_samples * output_dim, sizeof(float));

    if (!result->generated_samples) return NIMCP_ERROR_VAE_PLAST_NO_MEMORY;

    float total_recon_error = 0.0f;

    for (uint32_t s = 0; s < num_samples; s++) {
        /* Sample from prior (or use temperature) */
        float* z = nimcp_calloc(latent_dim, sizeof(float));
        if (!z) continue;

        float temp = bridge->config.consolidation.replay_temperature;
        for (uint32_t d = 0; d < latent_dim; d++) {
            /* Sample from N(0, temp) */
            float u1 = randf() + 1e-6f;
            float u2 = randf();
            z[d] = sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2) * temp;
        }

        /* Decode through VAE using tensor API */
        nimcp_tensor_t* z_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);
        nimcp_tensor_t* output_tensor = nimcp_tensor_create(&output_dim, 1, NIMCP_DTYPE_F32);

        if (z_tensor && output_tensor) {
            memcpy(TENSOR_DATA_F32(z_tensor), z, latent_dim * sizeof(float));
            if (vae_decode(bridge->vae, z_tensor, output_tensor) == 0) {
                memcpy(&result->generated_samples[s * output_dim],
                       TENSOR_DATA_F32(output_tensor), output_dim * sizeof(float));
                /* TODO: compute reconstruction loss if needed */
            }
        }
        nimcp_tensor_destroy(z_tensor);
        nimcp_tensor_destroy(output_tensor);
        nimcp_free(z);
        z = NULL;
    }

    result->avg_reconstruction_error = total_recon_error / (num_samples > 0 ? num_samples : 1);
    result->replay_time_us = (float)(get_timestamp_us() - start_us);

    bridge->stats.total_replays++;

    return 0;
}

int vae_plasticity_consolidate(vae_plasticity_bridge_t* bridge,
                                const vae_plasticity_replay_result_t* replay,
                                vae_plasticity_consolidation_result_t* result)
{
    if (!bridge || !replay || !result) return NIMCP_ERROR_VAE_PLAST_NULL;
    if (!bridge->plasticity_coordinator) return NIMCP_ERROR_VAE_PLAST_NOT_CONNECTED;

    uint64_t start_us = get_timestamp_us();
    memset(result, 0, sizeof(*result));

    bridge->state = VAE_PLAST_STATE_CONSOLIDATING;

    /* In a full implementation, this would:
     * 1. Feed replay samples through the network
     * 2. Apply plasticity rules with consolidation rate
     * 3. Track weight changes
     */

    result->synapses_updated = replay->num_samples * 100; /* Placeholder */
    result->avg_weight_change = 0.001f * bridge->config.consolidation.consolidation_rate;
    result->max_weight_change = 0.01f * bridge->config.consolidation.consolidation_rate;
    result->consolidation_error = replay->avg_reconstruction_error;
    result->consolidation_time_us = get_timestamp_us() - start_us;

    bridge->stats.total_consolidations++;

    bridge->state = VAE_PLAST_STATE_CONNECTED;
    return 0;
}

int vae_plasticity_add_to_replay_buffer(vae_plasticity_bridge_t* bridge,
                                         const float* sample, uint32_t dim)
{
    if (!bridge || !sample) return NIMCP_ERROR_VAE_PLAST_NULL;
    if (!bridge->replay_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_plasticity_add_to_replay_buffer: bridge->replay_buffer is NULL");
        return -1;
    }

    uint32_t offset = bridge->replay_buffer_head * dim;
    if (offset + dim <= bridge->replay_buffer_size) {
        memcpy(&bridge->replay_buffer[offset], sample, dim * sizeof(float));
        bridge->replay_buffer_head = (bridge->replay_buffer_head + 1) %
                                     (bridge->replay_buffer_size / dim);
    }

    return 0;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int vae_plasticity_add_mapping(vae_plasticity_bridge_t* bridge,
                                const vae_plasticity_mapping_t* mapping)
{
    if (!bridge || !mapping) return NIMCP_ERROR_VAE_PLAST_NULL;

    uint32_t new_count = bridge->config.num_mappings + 1;
    vae_plasticity_mapping_t* new_mappings = nimcp_realloc(
        bridge->config.mappings,
        new_count * sizeof(vae_plasticity_mapping_t));

    if (!new_mappings) return NIMCP_ERROR_VAE_PLAST_NO_MEMORY;

    bridge->config.mappings = new_mappings;
    bridge->config.mappings[bridge->config.num_mappings] = *mapping;
    bridge->config.num_mappings = new_count;

    return 0;
}

int vae_plasticity_set_global_rate(vae_plasticity_bridge_t* bridge, float rate)
{
    if (!bridge) return NIMCP_ERROR_VAE_PLAST_NULL;
    bridge->config.global_learning_rate = rate;
    return 0;
}

int vae_plasticity_enable_mechanism(vae_plasticity_bridge_t* bridge,
                                     vae_plasticity_mechanism_t mechanism,
                                     bool enabled)
{
    if (!bridge) return NIMCP_ERROR_VAE_PLAST_NULL;

    /* Track enabled mechanisms - could be stored as a bitmask */
    (void)mechanism;
    (void)enabled;

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_plasticity_bridge_state_t vae_plasticity_bridge_get_state(const vae_plasticity_bridge_t* bridge)
{
    if (!bridge) return VAE_PLAST_STATE_ERROR;
    return bridge->state;
}

int vae_plasticity_bridge_get_stats(const vae_plasticity_bridge_t* bridge,
                                     vae_plasticity_bridge_stats_t* stats)
{
    if (!bridge || !stats) return NIMCP_ERROR_VAE_PLAST_NULL;
    *stats = bridge->stats;
    return 0;
}

const char* vae_plasticity_mechanism_to_string(vae_plasticity_mechanism_t mech)
{
    switch (mech) {
        case VAE_PLAST_STDP: return "STDP";
        case VAE_PLAST_RSTDP: return "R-STDP";
        case VAE_PLAST_BCM: return "BCM";
        case VAE_PLAST_HOMEOSTATIC: return "homeostatic";
        case VAE_PLAST_STRUCTURAL: return "structural";
        case VAE_PLAST_METAPLASTICITY: return "metaplasticity";
        case VAE_PLAST_STP: return "STP";
        default: return "unknown";
    }
}

const char* vae_plasticity_signal_to_string(vae_plasticity_signal_t signal)
{
    switch (signal) {
        case VAE_PLAST_SIG_RECON_ERROR: return "recon_error";
        case VAE_PLAST_SIG_KL_DIVERGENCE: return "kl_divergence";
        case VAE_PLAST_SIG_PRECISION: return "precision";
        case VAE_PLAST_SIG_NOVELTY: return "novelty";
        case VAE_PLAST_SIG_ELBO: return "elbo";
        case VAE_PLAST_SIG_LATENT_CHANGE: return "latent_change";
        case VAE_PLAST_SIG_COLLAPSE_RISK: return "collapse_risk";
        default: return "unknown";
    }
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_plasticity_replay_result_free(vae_plasticity_replay_result_t* result)
{
    if (!result) return;
    nimcp_free(result->generated_samples);
    memset(result, 0, sizeof(*result));
}

void vae_plasticity_consolidation_result_free(vae_plasticity_consolidation_result_t* result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));
}
