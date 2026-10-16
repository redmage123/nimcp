/**
 * @file nimcp_vae.c
 * @brief Variational Autoencoder Core Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Main VAE system implementation that integrates encoder, decoder,
 * latent operations, and loss computation into a unified system.
 *
 * BIO_MODULE: 0x1F00
 */

#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/nimcp_vae_encoder.h"
#include "cognitive/vae/nimcp_vae_decoder.h"
#include "cognitive/vae/nimcp_vae_latent.h"
#include "cognitive/vae/nimcp_vae_loss.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
/* TODO: Fix immune path #include "immune/nimcp_immune.h" */
#include "utils/fault_tolerance/nimcp_health_agent.h"

#include <math.h>
#include <float.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define VAE_MODULE_ID           BIO_MODULE_VAE_CORE
#define VAE_EMA_ALPHA           0.99f
#define VAE_HEALTH_DECAY        0.001f
#define VAE_MIN_HEALTH          0.1f

/* ============================================================================
 * VAE System Structure
 * ============================================================================ */

/**
 * @brief Main VAE system structure
 */
struct vae_system {
    /* Configuration */
    vae_config_t config;

    /* Sub-components */
    vae_encoder_t* encoder;
    vae_decoder_t* decoder;
    vae_loss_ctx_t* loss_ctx;

    /* State */
    vae_state_t state;
    bool is_training;
    bool is_initialized;

    /* Current latent state */
    vae_latent_state_t current_latent;

    /* Latest loss values */
    vae_loss_t latest_loss;

    /* Statistics */
    vae_stats_t stats;

    /* Health */
    vae_health_t health;

    /* Cached tensors for reuse */
    nimcp_tensor_t* cached_mu;
    nimcp_tensor_t* cached_log_var;
    nimcp_tensor_t* cached_z;
    nimcp_tensor_t* cached_recon;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Health agent */
    nimcp_health_agent_t* health_agent;
};
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

/* Timestamp utility stub */
#include <time.h>
static inline uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* Stub heartbeat for migration compatibility */
static inline void vae_heartbeat(void* vae, const char* op) {
    (void)vae; (void)op;
}

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_vae_health_agent = NULL;

#include "utils/bridge/nimcp_bridge_boilerplate.h"
BRIDGE_DEFINE_MESH_REGISTRATION(vae, MESH_ADAPTER_CATEGORY_COGNITIVE)


/**
 * @brief Update health metrics
 */
static void vae_update_health(vae_system_t* vae, bool success)
{
    if (!vae) return;

    uint64_t now = get_timestamp_us();

    if (success) {
        vae->health.consecutive_errors = 0;
        vae->health.last_healthy_time_us = now;

        /* Recover health gradually */
        vae->health.encoder_health = fminf(1.0f, vae->health.encoder_health + 0.01f);
        vae->health.decoder_health = fminf(1.0f, vae->health.decoder_health + 0.01f);
        vae->health.latent_health = fminf(1.0f, vae->health.latent_health + 0.01f);
        vae->health.training_health = fminf(1.0f, vae->health.training_health + 0.01f);
    } else {
        vae->health.consecutive_errors++;
        vae->health.last_error_time_us = now;

        /* Degrade health */
        vae->health.encoder_health = fmaxf(VAE_MIN_HEALTH, vae->health.encoder_health - 0.1f);
        vae->health.decoder_health = fmaxf(VAE_MIN_HEALTH, vae->health.decoder_health - 0.1f);
        vae->health.latent_health = fmaxf(VAE_MIN_HEALTH, vae->health.latent_health - 0.1f);
        vae->health.training_health = fmaxf(VAE_MIN_HEALTH, vae->health.training_health - 0.1f);
    }

    /* Compute overall health */
    vae->health.overall_health = (vae->health.encoder_health +
                                   vae->health.decoder_health +
                                   vae->health.latent_health +
                                   vae->health.training_health) / 4.0f;

    vae->health.is_healthy = (vae->health.overall_health > 0.5f) &&
                             (vae->health.consecutive_errors < vae->config.max_consecutive_errors);
}

/**
 * @brief Update EMA statistics
 */
static void vae_update_ema_stats(vae_system_t* vae, const vae_loss_t* loss)
{
    if (!vae || !loss) return;

    float alpha = VAE_EMA_ALPHA;

    if (vae->stats.total_forward_calls == 0) {
        vae->stats.ema_total_loss = loss->total_loss;
        vae->stats.ema_reconstruction_loss = loss->reconstruction_loss;
        vae->stats.ema_kl_divergence = loss->kl_divergence;
        vae->stats.ema_free_energy = loss->free_energy;
    } else {
        vae->stats.ema_total_loss = alpha * vae->stats.ema_total_loss + (1.0f - alpha) * loss->total_loss;
        vae->stats.ema_reconstruction_loss = alpha * vae->stats.ema_reconstruction_loss + (1.0f - alpha) * loss->reconstruction_loss;
        vae->stats.ema_kl_divergence = alpha * vae->stats.ema_kl_divergence + (1.0f - alpha) * loss->kl_divergence;
        vae->stats.ema_free_energy = alpha * vae->stats.ema_free_energy + (1.0f - alpha) * loss->free_energy;
    }
}

/**
 * @brief Set VAE state with logging
 */
static void vae_set_state(vae_system_t* vae, vae_state_t new_state)
{
    if (!vae) return;
    vae_state_t old_state = vae->state;
    vae->state = new_state;

    if (old_state != new_state) {
        NIMCP_LOG_DEBUG("VAE: State transition %s -> %s",
                        vae_state_to_string(old_state),
                        vae_state_to_string(new_state));
    }
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int vae_default_config(vae_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NULL_POINTER,
                             "NULL config in vae_default_config");
        return -1;
    }

    memset(config, 0, sizeof(vae_config_t));

    /* Encoder defaults */
    config->encoder.input_dim = 784;  /* MNIST-like default */
    config->encoder.latent_dim = 32;
    config->encoder.num_layers = 2;
    config->encoder.layers[0].units = 256;
    config->encoder.layers[0].activation = VAE_ACTIVATION_RELU;
    config->encoder.layers[0].dropout_rate = 0.0f;
    config->encoder.layers[0].batch_norm = false;
    config->encoder.layers[0].use_bias = true;
    config->encoder.layers[1].units = 128;
    config->encoder.layers[1].activation = VAE_ACTIVATION_RELU;
    config->encoder.layers[1].dropout_rate = 0.0f;
    config->encoder.layers[1].batch_norm = false;
    config->encoder.layers[1].use_bias = true;
    config->encoder.mu_activation = VAE_ACTIVATION_LINEAR;
    config->encoder.var_activation = VAE_ACTIVATION_LINEAR;

    /* Decoder defaults (mirror of encoder) */
    config->decoder.latent_dim = 32;
    config->decoder.output_dim = 784;
    config->decoder.num_layers = 2;
    config->decoder.layers[0].units = 128;
    config->decoder.layers[0].activation = VAE_ACTIVATION_RELU;
    config->decoder.layers[0].dropout_rate = 0.0f;
    config->decoder.layers[0].batch_norm = false;
    config->decoder.layers[0].use_bias = true;
    config->decoder.layers[1].units = 256;
    config->decoder.layers[1].activation = VAE_ACTIVATION_RELU;
    config->decoder.layers[1].dropout_rate = 0.0f;
    config->decoder.layers[1].batch_norm = false;
    config->decoder.layers[1].use_bias = true;
    config->decoder.final_activation = VAE_ACTIVATION_SIGMOID;
    config->decoder.output_variance = false;

    /* Training defaults */
    config->training.learning_rate = VAE_DEFAULT_LEARNING_RATE;
    config->training.beta = VAE_DEFAULT_BETA;
    config->training.beta_min = 0.0f;
    config->training.beta_max = 1.0f;
    config->training.beta_warmup_steps = 0;
    config->training.loss_type = VAE_LOSS_MSE;
    config->training.gradient_clip = VAE_DEFAULT_GRADIENT_CLIP;
    config->training.free_bits = false;
    config->training.free_bits_lambda = 0.0f;
    config->training.batch_size = VAE_DEFAULT_BATCH_SIZE;
    config->training.weight_decay = 0.0f;
    config->training.momentum = NIMCP_MOMENTUM_DEFAULT;

    /* Variant */
    config->variant = VAE_VARIANT_STANDARD;
    config->prior_type = VAE_PRIOR_STANDARD_NORMAL;

    /* Integration flags */
    config->enable_immune_integration = true;
    config->enable_bio_async = false;
    config->enable_kg_wiring = false;
    config->enable_logging = true;

    /* Thresholds */
    config->anomaly_threshold = 2.0f;
    config->collapse_threshold = VAE_LATENT_COLLAPSE_THRESHOLD;

    /* Health monitoring */
    config->heartbeat_interval_ms = 100;
    config->max_consecutive_errors = 5;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

vae_system_t* vae_create(const vae_config_t* config)
{
    vae_config_t default_config;

    if (!config) {
        if (vae_default_config(&default_config) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_create: validation failed");
            return NULL;
        }
        config = &default_config;
    }

    /* Validate configuration */
    if (config->encoder.input_dim == 0 || config->encoder.input_dim > VAE_MAX_IO_DIM) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_INVALID_DIM,
                             "Invalid encoder input dimension");
        return NULL;
    }

    if (config->encoder.latent_dim == 0 || config->encoder.latent_dim > VAE_MAX_LATENT_DIM) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_INVALID_DIM,
                             "Invalid latent dimension");
        return NULL;
    }

    /* Allocate system */
    vae_system_t* vae = nimcp_calloc(1, sizeof(vae_system_t));
    if (!vae) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NO_MEMORY,
                             "Failed to allocate VAE system");
        return NULL;
    }

    vae->config = *config;
    vae->state = VAE_STATE_UNINITIALIZED;
    vae->is_training = false;

    /* Create mutex */
    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    vae->mutex = nimcp_mutex_create(&attr);
    if (!vae->mutex) {
        NIMCP_LOG_ERROR("VAE: Failed to create mutex");
        nimcp_free(vae);
        vae = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_create: vae->mutex is NULL");
        return NULL;
    }

    /* Create encoder - use config directly as types match */
    vae->encoder = vae_encoder_create(&config->encoder);
    if (!vae->encoder) {
        NIMCP_LOG_ERROR("VAE: Failed to create encoder");
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_ENCODER_FAILED,
                             "Failed to create encoder");
        nimcp_mutex_destroy(vae->mutex);
        nimcp_free(vae);
        vae = NULL;
        return NULL;
    }

    /* Create decoder - use config directly as types match */
    vae->decoder = vae_decoder_create(&config->decoder);
    if (!vae->decoder) {
        NIMCP_LOG_ERROR("VAE: Failed to create decoder");
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_DECODER_FAILED,
                             "Failed to create decoder");
        vae_encoder_destroy(vae->encoder);
        nimcp_mutex_destroy(vae->mutex);
        nimcp_free(vae);
        vae = NULL;
        return NULL;
    }

    /* Create loss context */
    vae_loss_config_t loss_config = vae_loss_default_config();
    loss_config.beta = config->training.beta;
    loss_config.free_bits = config->training.free_bits ? config->training.free_bits_lambda : 0.0f;

    switch (config->training.loss_type) {
        case VAE_LOSS_MSE:
            loss_config.recon_type = VAE_RECON_MSE;
            break;
        case VAE_LOSS_BCE:
            loss_config.recon_type = VAE_RECON_BCE;
            break;
        case VAE_LOSS_GAUSSIAN:
            loss_config.recon_type = VAE_RECON_GAUSSIAN_NLL;
            break;
        default:
            loss_config.recon_type = VAE_RECON_MSE;
    }

    if (config->training.beta_warmup_steps > 0) {
        loss_config.use_kl_annealing = true;
        loss_config.kl_anneal_start = config->training.beta_min;
        loss_config.kl_anneal_end = config->training.beta_max;
        loss_config.kl_anneal_steps = config->training.beta_warmup_steps;
    }

    vae->loss_ctx = vae_loss_ctx_create(&loss_config);
    if (!vae->loss_ctx) {
        NIMCP_LOG_ERROR("VAE: Failed to create loss context");
        vae_decoder_destroy(vae->decoder);
        vae_encoder_destroy(vae->encoder);
        nimcp_mutex_destroy(vae->mutex);
        nimcp_free(vae);
        vae = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_create: vae->loss_ctx is NULL");
        return NULL;
    }

    /* Initialize latent state */
    if (vae_latent_state_init(&vae->current_latent, config->encoder.latent_dim) != 0) {
        NIMCP_LOG_ERROR("VAE: Failed to initialize latent state");
        vae_loss_ctx_destroy(vae->loss_ctx);
        vae_decoder_destroy(vae->decoder);
        vae_encoder_destroy(vae->encoder);
        nimcp_mutex_destroy(vae->mutex);
        nimcp_free(vae);
        vae = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "vae_create: validation failed");
        return NULL;
    }

    /* Initialize health */
    vae->health.encoder_health = 1.0f;
    vae->health.decoder_health = 1.0f;
    vae->health.latent_health = 1.0f;
    vae->health.training_health = 1.0f;
    vae->health.overall_health = 1.0f;
    vae->health.is_healthy = true;
    vae->health.consecutive_errors = 0;
    vae->health.last_healthy_time_us = get_timestamp_us();

    /* Initialize weights */
    uint64_t seed = (uint64_t)time(NULL);
    vae_encoder_init_weights(vae->encoder, seed);
    vae_decoder_init_weights(vae->decoder, seed + 1);

    vae->is_initialized = true;
    vae_set_state(vae, VAE_STATE_IDLE);

    NIMCP_LOG_INFO("VAE: System created (input=%u, latent=%u, output=%u)",
                   config->encoder.input_dim,
                   config->encoder.latent_dim,
                   config->decoder.output_dim);

    vae_heartbeat(vae, "create");

    return vae;
}

void vae_destroy(vae_system_t* vae)
{
    if (!vae) return;

    NIMCP_LOG_INFO("VAE: Destroying system");

    /* Free cached tensors */
    if (vae->cached_mu) nimcp_tensor_destroy(vae->cached_mu);
    if (vae->cached_log_var) nimcp_tensor_destroy(vae->cached_log_var);
    if (vae->cached_z) nimcp_tensor_destroy(vae->cached_z);
    if (vae->cached_recon) nimcp_tensor_destroy(vae->cached_recon);

    /* Free latent state */
    vae_latent_state_free(&vae->current_latent);

    /* Free components */
    if (vae->loss_ctx) vae_loss_ctx_destroy(vae->loss_ctx);
    if (vae->decoder) vae_decoder_destroy(vae->decoder);
    if (vae->encoder) vae_encoder_destroy(vae->encoder);

    /* Free mutex */
    if (vae->mutex) nimcp_mutex_destroy(vae->mutex);

    nimcp_free(vae);
    vae = NULL;
}

int vae_reset(vae_system_t* vae)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NOT_INITIALIZED,
                             "VAE not initialized for reset");
        return -1;
    }

    nimcp_mutex_lock(vae->mutex);

    /* Reset encoder and decoder */
    if (vae->encoder) vae_encoder_reset(vae->encoder);
    if (vae->decoder) vae_decoder_reset(vae->decoder);

    /* Reset loss context */
    if (vae->loss_ctx) vae_loss_ctx_reset(vae->loss_ctx);

    /* Reset latent state */
    vae_latent_state_reset(&vae->current_latent);

    /* Reset statistics */
    memset(&vae->stats, 0, sizeof(vae_stats_t));

    /* Reset health */
    vae->health.encoder_health = 1.0f;
    vae->health.decoder_health = 1.0f;
    vae->health.latent_health = 1.0f;
    vae->health.training_health = 1.0f;
    vae->health.overall_health = 1.0f;
    vae->health.is_healthy = true;
    vae->health.consecutive_errors = 0;
    vae->health.last_healthy_time_us = get_timestamp_us();

    /* Reset state */
    vae_set_state(vae, VAE_STATE_IDLE);
    vae->is_training = false;

    nimcp_mutex_unlock(vae->mutex);

    NIMCP_LOG_INFO("VAE: System reset");
    vae_heartbeat(vae, "reset");

    return 0;
}

/* ============================================================================
 * Core Processing API
 * ============================================================================ */

int vae_encode(vae_system_t* vae,
               const nimcp_tensor_t* input,
               nimcp_tensor_t* mu,
               nimcp_tensor_t* log_var)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NOT_INITIALIZED,
                             "VAE not initialized for encode");
        return -1;
    }

    if (!input || !mu || !log_var) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NULL_POINTER,
                             "NULL tensor in vae_encode");
        return -1;
    }

    nimcp_mutex_lock(vae->mutex);
    vae_set_state(vae, VAE_STATE_ENCODING);

    uint64_t start_time = get_timestamp_us();

    /* Forward through encoder */
    int result = vae_encoder_forward(vae->encoder, input, mu, log_var);

    if (result != 0) {
        NIMCP_LOG_ERROR("VAE: Encoder forward failed");
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_ENCODER_FAILED,
                             "Encoder forward pass failed");
        vae_update_health(vae, false);
        vae_set_state(vae, VAE_STATE_ERROR);
        nimcp_mutex_unlock(vae->mutex);
        return -1;
    }

    /* Update statistics */
    uint64_t elapsed = get_timestamp_us() - start_time;
    vae->stats.total_encode_calls++;
    vae->stats.avg_encode_latency_us = (vae->stats.avg_encode_latency_us *
                                        (vae->stats.total_encode_calls - 1) +
                                        (float)elapsed) / vae->stats.total_encode_calls;

    vae_update_health(vae, true);
    vae_set_state(vae, VAE_STATE_IDLE);

    nimcp_mutex_unlock(vae->mutex);

    vae_heartbeat(vae, "encode");

    return 0;
}

int vae_sample(vae_system_t* vae,
               const nimcp_tensor_t* mu,
               const nimcp_tensor_t* log_var,
               nimcp_tensor_t* z)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NOT_INITIALIZED,
                             "VAE not initialized for sample");
        return -1;
    }

    if (!mu || !log_var || !z) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NULL_POINTER,
                             "NULL tensor in vae_sample");
        return -1;
    }

    nimcp_mutex_lock(vae->mutex);
    vae_set_state(vae, VAE_STATE_SAMPLING);

    /* Sample using reparameterization trick */
    int result = vae_latent_sample(mu, log_var, z);

    if (result != 0) {
        NIMCP_LOG_ERROR("VAE: Latent sampling failed");
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_SAMPLE_FAILED,
                             "Latent sampling failed");
        vae_update_health(vae, false);
        vae_set_state(vae, VAE_STATE_ERROR);
        nimcp_mutex_unlock(vae->mutex);
        return -1;
    }

    /* Check for variance issues */
    uint32_t num_collapsed = 0;
    uint32_t num_exploded = 0;

    if (vae_latent_check_collapse(mu, log_var, vae->config.collapse_threshold, &num_collapsed)) {
        vae->stats.posterior_collapses++;
        vae->stats.collapsed_dimensions = num_collapsed;

        if (vae->config.enable_immune_integration) {
            NIMCP_LOG_WARN("VAE: Posterior collapse detected (%u dimensions)", num_collapsed);
        }
    }

    if (vae_latent_check_explosion(log_var, VAE_LATENT_EXPLODE_THRESHOLD, &num_exploded)) {
        vae->stats.variance_explosions++;

        if (vae->config.enable_immune_integration) {
            NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_VARIANCE_EXPLODE,
                                 "Variance explosion detected");
        }
    }

    vae_update_health(vae, true);
    vae_set_state(vae, VAE_STATE_IDLE);

    nimcp_mutex_unlock(vae->mutex);

    return 0;
}

int vae_decode(vae_system_t* vae,
               const nimcp_tensor_t* z,
               nimcp_tensor_t* reconstruction)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NOT_INITIALIZED,
                             "VAE not initialized for decode");
        return -1;
    }

    if (!z || !reconstruction) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NULL_POINTER,
                             "NULL tensor in vae_decode");
        return -1;
    }

    nimcp_mutex_lock(vae->mutex);
    vae_set_state(vae, VAE_STATE_DECODING);

    uint64_t start_time = get_timestamp_us();

    /* Forward through decoder */
    int result = vae_decoder_forward(vae->decoder, z, reconstruction);

    if (result != 0) {
        NIMCP_LOG_ERROR("VAE: Decoder forward failed");
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_DECODER_FAILED,
                             "Decoder forward pass failed");
        vae_update_health(vae, false);
        vae_set_state(vae, VAE_STATE_ERROR);
        nimcp_mutex_unlock(vae->mutex);
        return -1;
    }

    /* Update statistics */
    uint64_t elapsed = get_timestamp_us() - start_time;
    vae->stats.total_decode_calls++;
    vae->stats.avg_decode_latency_us = (vae->stats.avg_decode_latency_us *
                                        (vae->stats.total_decode_calls - 1) +
                                        (float)elapsed) / vae->stats.total_decode_calls;

    vae_update_health(vae, true);
    vae_set_state(vae, VAE_STATE_IDLE);

    nimcp_mutex_unlock(vae->mutex);

    vae_heartbeat(vae, "decode");

    return 0;
}

int vae_forward(vae_system_t* vae,
                const nimcp_tensor_t* input,
                nimcp_tensor_t* reconstruction,
                vae_latent_state_t* latent)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NOT_INITIALIZED,
                             "VAE not initialized for forward");
        return -1;
    }

    if (!input) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NULL_POINTER,
                             "NULL input in vae_forward");
        return -1;
    }

    nimcp_mutex_lock(vae->mutex);

    /* Get dimensions */
    uint32_t batch_size = (input->shape.rank >= 1) ? input->shape.dims[0] : 1;
    uint32_t latent_dim = vae->config.encoder.latent_dim;
    uint32_t output_dim = vae->config.decoder.output_dim;

    /* Allocate or reuse temporary tensors */
    uint32_t latent_dims[2] = {batch_size, latent_dim};
    uint32_t output_dims[2] = {batch_size, output_dim};

    nimcp_tensor_t* mu = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* z = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_F32);

    if (!mu || !log_var || !z) {
        if (mu) nimcp_tensor_destroy(mu);
        if (log_var) nimcp_tensor_destroy(log_var);
        if (z) nimcp_tensor_destroy(z);
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NO_MEMORY,
                             "Failed to allocate tensors for forward");
        nimcp_mutex_unlock(vae->mutex);
        return -1;
    }

    int result = 0;

    /* Encode */
    vae_set_state(vae, VAE_STATE_ENCODING);
    result = vae_encoder_forward(vae->encoder, input, mu, log_var);
    if (result != 0) {
        NIMCP_LOG_ERROR("VAE: Forward encode failed");
        goto cleanup;
    }

    /* Sample */
    vae_set_state(vae, VAE_STATE_SAMPLING);
    result = vae_latent_sample(mu, log_var, z);
    if (result != 0) {
        NIMCP_LOG_ERROR("VAE: Forward sample failed");
        goto cleanup;
    }

    /* Decode (if reconstruction requested) */
    if (reconstruction) {
        vae_set_state(vae, VAE_STATE_DECODING);
        result = vae_decoder_forward(vae->decoder, z, reconstruction);
        if (result != 0) {
            NIMCP_LOG_ERROR("VAE: Forward decode failed");
            goto cleanup;
        }
    }

    /* Update internal latent state */
    vae_latent_state_update(&vae->current_latent, mu, log_var, z);

    /* Copy to output latent if requested */
    if (latent) {
        if (latent->mu && mu->data) {
            memcpy(latent->mu, mu->data, latent_dim * sizeof(float));
        }
        if (latent->log_var && log_var->data) {
            memcpy(latent->log_var, log_var->data, latent_dim * sizeof(float));
        }
        if (latent->z && z->data) {
            memcpy(latent->z, z->data, latent_dim * sizeof(float));
        }
        latent->latent_dim = latent_dim;
        latent->is_valid = true;

        /* Compute precision */
        if (latent->precision) {
            for (uint32_t i = 0; i < latent_dim; i++) {
                float var = expf(((float*)log_var->data)[i]);
                latent->precision[i] = 1.0f / fmaxf(var, VAE_MIN_VARIANCE);
            }
        }
    }

    vae->stats.total_forward_calls++;
    vae_update_health(vae, true);

cleanup:
    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(z);

    vae_set_state(vae, result == 0 ? VAE_STATE_IDLE : VAE_STATE_ERROR);
    nimcp_mutex_unlock(vae->mutex);

    if (result == 0) {
        vae_heartbeat(vae, "forward");
    }

    return result;
}

/* Forward declaration for unlocked variant used by vae_train_step */
static int vae_compute_loss_unlocked(vae_system_t* vae,
                     const nimcp_tensor_t* input,
                     const nimcp_tensor_t* reconstruction,
                     const nimcp_tensor_t* mu,
                     const nimcp_tensor_t* log_var,
                     vae_loss_t* loss);

int vae_compute_loss(vae_system_t* vae,
                     const nimcp_tensor_t* input,
                     const nimcp_tensor_t* reconstruction,
                     const nimcp_tensor_t* mu,
                     const nimcp_tensor_t* log_var,
                     vae_loss_t* loss)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NOT_INITIALIZED,
                             "VAE not initialized for loss computation");
        return -1;
    }

    if (!input || !reconstruction || !mu || !log_var || !loss) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NULL_POINTER,
                             "NULL argument in vae_compute_loss");
        return -1;
    }

    nimcp_mutex_lock(vae->mutex);
    int ret = vae_compute_loss_unlocked(vae, input, reconstruction, mu, log_var, loss);
    nimcp_mutex_unlock(vae->mutex);
    return ret;
}

/** Internal: compute loss without locking (caller must hold vae->mutex) */
static int vae_compute_loss_unlocked(vae_system_t* vae,
                     const nimcp_tensor_t* input,
                     const nimcp_tensor_t* reconstruction,
                     const nimcp_tensor_t* mu,
                     const nimcp_tensor_t* log_var,
                     vae_loss_t* loss)
{
    /* Create breakdown for detailed results */
    uint32_t latent_dim = vae->config.encoder.latent_dim;
    vae_loss_breakdown_t* breakdown = vae_loss_breakdown_create(latent_dim);

    if (!breakdown) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_compute_loss: breakdown is NULL");
        return -1;
    }

    /* Compute loss */
    float total = vae_loss_compute(vae->loss_ctx, input, reconstruction, mu, log_var, breakdown);

    if (vae_loss_is_invalid(total)) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_LOSS_NAN,
                             "Loss computation returned NaN/Inf");
        vae->stats.anomalies_detected++;
        vae_loss_breakdown_free(breakdown);
        return -1;
    }

    /* Fill output structure */
    loss->total_loss = breakdown->total_loss;
    loss->reconstruction_loss = breakdown->recon_loss;
    loss->kl_divergence = breakdown->kl_raw;
    loss->weighted_kl = breakdown->kl_loss;
    loss->free_energy = breakdown->free_energy;
    loss->elbo = breakdown->elbo;
    loss->inaccuracy = breakdown->recon_loss;  /* FEP inaccuracy */
    loss->complexity = breakdown->kl_raw;       /* FEP complexity */
    loss->active_units = breakdown->active_units;

    /* Store latest loss */
    vae->latest_loss = *loss;

    /* Update EMA statistics */
    vae_update_ema_stats(vae, loss);

    vae_loss_breakdown_free(breakdown);

    return 0;
}

/* ============================================================================
 * Training API
 * ============================================================================ */

int vae_train_step(vae_system_t* vae,
                   const nimcp_tensor_t* input,
                   vae_loss_t* loss)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NOT_INITIALIZED,
                             "VAE not initialized for training");
        return -1;
    }

    if (!input) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NULL_POINTER,
                             "NULL input in vae_train_step");
        return -1;
    }

    nimcp_mutex_lock(vae->mutex);
    vae_set_state(vae, VAE_STATE_TRAINING);

    /* Ensure training mode */
    vae_encoder_set_training(vae->encoder, true);
    vae_decoder_set_training(vae->decoder, true);

    /* Get dimensions */
    uint32_t batch_size = (input->shape.rank >= 1) ? input->shape.dims[0] : 1;
    uint32_t latent_dim = vae->config.encoder.latent_dim;
    uint32_t output_dim = vae->config.decoder.output_dim;

    /* Allocate tensors */
    uint32_t latent_dims[2] = {batch_size, latent_dim};
    uint32_t output_dims[2] = {batch_size, output_dim};

    nimcp_tensor_t* mu = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* z = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* recon = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_F32);

    if (!mu || !log_var || !z || !recon) {
        if (mu) nimcp_tensor_destroy(mu);
        if (log_var) nimcp_tensor_destroy(log_var);
        if (z) nimcp_tensor_destroy(z);
        if (recon) nimcp_tensor_destroy(recon);
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NO_MEMORY,
                             "Failed to allocate tensors for training");
        vae_set_state(vae, VAE_STATE_ERROR);
        nimcp_mutex_unlock(vae->mutex);
        return -1;
    }

    int result = 0;

    /* Zero gradients */
    vae_encoder_zero_grad(vae->encoder);
    vae_decoder_zero_grad(vae->decoder);

    /* Forward pass */
    result = vae_encoder_forward(vae->encoder, input, mu, log_var);
    if (result != 0) goto cleanup;

    result = vae_latent_sample(mu, log_var, z);
    if (result != 0) goto cleanup;

    result = vae_decoder_forward(vae->decoder, z, recon);
    if (result != 0) goto cleanup;

    /* Compute loss — use unlocked variant since we already hold vae->mutex */
    vae_loss_t train_loss;
    result = vae_compute_loss_unlocked(vae, input, recon, mu, log_var, &train_loss);
    if (result != 0) goto cleanup;

    /* Copy loss to output */
    if (loss) {
        *loss = train_loss;
    }

    /* Backward pass */
    /* Compute reconstruction gradient */
    nimcp_tensor_t* d_recon = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* d_z = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* d_mu = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* d_log_var = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_F32);

    if (!d_recon || !d_z || !d_mu || !d_log_var) {
        if (d_recon) nimcp_tensor_destroy(d_recon);
        if (d_z) nimcp_tensor_destroy(d_z);
        if (d_mu) nimcp_tensor_destroy(d_mu);
        if (d_log_var) nimcp_tensor_destroy(d_log_var);
        result = -1;
        goto cleanup;
    }

    /* Get reconstruction gradient */
    vae_loss_recon_gradient(input, recon, vae->loss_ctx->config.recon_type, d_recon);

    /* Backward through decoder */
    result = vae_decoder_backward(vae->decoder, d_recon, d_z);
    if (result != 0) {
        nimcp_tensor_destroy(d_recon);
        nimcp_tensor_destroy(d_z);
        nimcp_tensor_destroy(d_mu);
        nimcp_tensor_destroy(d_log_var);
        goto cleanup;
    }

    /* Add KL gradient to mu and log_var */
    float beta = vae_loss_get_kl_weight(vae->loss_ctx);
    vae_loss_kl_gradient(mu, log_var, beta, d_mu, d_log_var);

    /* Backward through encoder */
    (void)d_z; /* Not needed for current encoder backward */
    result = vae_encoder_backward(vae->encoder, d_mu, d_log_var, NULL);

    nimcp_tensor_destroy(d_recon);
    nimcp_tensor_destroy(d_z);
    nimcp_tensor_destroy(d_mu);
    nimcp_tensor_destroy(d_log_var);

    if (result != 0) goto cleanup;

    /* Check for NaN gradients */
    if (vae_encoder_has_nan(vae->encoder) || vae_decoder_has_nan(vae->decoder)) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_GRADIENT_NAN,
                             "NaN detected in gradients");
        vae->stats.gradient_nans++;
        result = -1;
        goto cleanup;
    }

    /* Clip gradients */
    float clip_threshold = vae->config.training.gradient_clip;
    vae_encoder_clip_gradients(vae->encoder, clip_threshold);
    vae_decoder_clip_gradients(vae->decoder, clip_threshold);

    /* Apply gradients */
    float lr = vae->config.training.learning_rate;
    vae_encoder_apply_gradients(vae->encoder, lr);
    vae_decoder_apply_gradients(vae->decoder, lr);

    /* Update annealing */
    vae_loss_anneal_step(vae->loss_ctx);

    /* Update statistics */
    vae->stats.total_train_steps++;
    vae->stats.global_step++;
    vae->stats.current_beta = vae_loss_get_kl_weight(vae->loss_ctx);
    vae->stats.current_lr = lr;

    vae_update_health(vae, true);

cleanup:
    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(z);
    nimcp_tensor_destroy(recon);

    vae_set_state(vae, result == 0 ? VAE_STATE_IDLE : VAE_STATE_ERROR);
    nimcp_mutex_unlock(vae->mutex);

    if (result == 0) {
        vae_heartbeat(vae, "train_step");
    }

    return result;
}

int vae_set_training(vae_system_t* vae, bool training)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_set_training: required parameter is NULL (vae, vae->is_initialized)");
        return -1;
    }

    nimcp_mutex_lock(vae->mutex);

    vae->is_training = training;
    vae_encoder_set_training(vae->encoder, training);
    vae_decoder_set_training(vae->decoder, training);

    nimcp_mutex_unlock(vae->mutex);

    NIMCP_LOG_DEBUG("VAE: Training mode set to %s", training ? "true" : "false");

    return 0;
}

bool vae_is_training(const vae_system_t* vae)
{
    if (!vae) {
        return false;
    }
    return vae->is_training;
}

int vae_set_beta(vae_system_t* vae, float beta)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_set_beta: required parameter is NULL (vae, vae->is_initialized)");
        return -1;
    }
    if (beta < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_set_beta: validation failed");
        return -1;
    }

    nimcp_mutex_lock(vae->mutex);
    if (!vae->loss_ctx) {
        nimcp_mutex_unlock(vae->mutex);
        return -1;
    }
    vae->loss_ctx->config.beta = beta;
    vae->loss_ctx->current_kl_weight = beta;
    vae->stats.current_beta = beta;
    nimcp_mutex_unlock(vae->mutex);

    NIMCP_LOG_DEBUG("VAE: Beta set to %.4f", beta);

    return 0;
}

float vae_get_beta(const vae_system_t* vae)
{
    if (!vae) return NAN;
    return vae->stats.current_beta;
}

int vae_set_learning_rate(vae_system_t* vae, float lr)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_set_learning_rate: required parameter is NULL (vae, vae->is_initialized)");
        return -1;
    }
    if (lr <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_set_learning_rate: validation failed");
        return -1;
    }

    nimcp_mutex_lock(vae->mutex);
    vae->config.training.learning_rate = lr;
    vae->stats.current_lr = lr;
    nimcp_mutex_unlock(vae->mutex);

    NIMCP_LOG_DEBUG("VAE: Learning rate set to %.6f", lr);

    return 0;
}

/* ============================================================================
 * Generation API
 * ============================================================================ */

int vae_generate(vae_system_t* vae,
                 uint32_t num_samples,
                 nimcp_tensor_t* samples)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NOT_INITIALIZED,
                             "VAE not initialized for generation");
        return -1;
    }

    if (!samples || num_samples == 0) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_MODULE_ID, NIMCP_ERROR_VAE_NULL_POINTER,
                             "Invalid samples parameter in vae_generate");
        return -1;
    }

    nimcp_mutex_lock(vae->mutex);
    vae_set_state(vae, VAE_STATE_GENERATING);

    uint32_t latent_dim = vae->config.encoder.latent_dim;
    uint32_t latent_dims[2] = {num_samples, latent_dim};

    /* Sample from prior */
    nimcp_tensor_t* z = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_F32);
    if (!z) {
        vae_set_state(vae, VAE_STATE_ERROR);
        nimcp_mutex_unlock(vae->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_generate: z is NULL");
        return -1;
    }

    int result = vae_latent_sample_prior(num_samples, latent_dim, z);
    if (result != 0) {
        nimcp_tensor_destroy(z);
        vae_set_state(vae, VAE_STATE_ERROR);
        nimcp_mutex_unlock(vae->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_generate: validation failed");
        return -1;
    }

    /* Decode */
    result = vae_decoder_forward(vae->decoder, z, samples);

    nimcp_tensor_destroy(z);

    vae->stats.total_generate_calls++;
    vae_set_state(vae, result == 0 ? VAE_STATE_IDLE : VAE_STATE_ERROR);
    nimcp_mutex_unlock(vae->mutex);

    if (result == 0) {
        vae_heartbeat(vae, "generate");
    }

    return result;
}

int vae_sample_prior(vae_system_t* vae,
                     uint32_t num_samples,
                     nimcp_tensor_t* latent_samples)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_sample_prior: required parameter is NULL (vae, vae->is_initialized)");
        return -1;
    }
    if (!latent_samples || num_samples == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_sample_prior: latent_samples is NULL");
        return -1;
    }

    uint32_t latent_dim = vae->config.encoder.latent_dim;
    return vae_latent_sample_prior(num_samples, latent_dim, latent_samples);
}

int vae_interpolate(vae_system_t* vae,
                    const nimcp_tensor_t* z1,
                    const nimcp_tensor_t* z2,
                    uint32_t num_steps,
                    nimcp_tensor_t* interpolations)
{
    if (!vae || !vae->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_interpolate: required parameter is NULL (vae, vae->is_initialized)");
        return -1;
    }
    if (!z1 || !z2 || !interpolations || num_steps == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_interpolate: required parameter is NULL (z1, z2, interpolations)");
        return -1;
    }

    nimcp_mutex_lock(vae->mutex);

    uint32_t latent_dim = vae->config.encoder.latent_dim;
    uint32_t output_dim = vae->config.decoder.output_dim;

    /* Create interpolation path in latent space */
    uint32_t path_dims[2] = {num_steps, latent_dim};
    nimcp_tensor_t* path = nimcp_tensor_create(path_dims, 2, NIMCP_DTYPE_F32);
    if (!path) {
        nimcp_mutex_unlock(vae->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_interpolate: path is NULL");
        return -1;
    }

    int result = vae_latent_interpolate_path(z1, z2, num_steps, true, path);
    if (result != 0) {
        nimcp_tensor_destroy(path);
        nimcp_mutex_unlock(vae->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_interpolate: validation failed");
        return -1;
    }

    /* Decode each point */
    result = vae_decoder_forward(vae->decoder, path, interpolations);

    nimcp_tensor_destroy(path);
    nimcp_mutex_unlock(vae->mutex);

    return result;
}

int vae_reconstruct(vae_system_t* vae,
                    const nimcp_tensor_t* input,
                    nimcp_tensor_t* reconstruction)
{
    return vae_forward(vae, input, reconstruction, NULL);
}

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_state_t vae_get_state(const vae_system_t* vae)
{
    if (!vae) return VAE_STATE_UNINITIALIZED;
    return vae->state;
}

const char* vae_state_to_string(vae_state_t state)
{
    switch (state) {
        case VAE_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case VAE_STATE_IDLE:          return "IDLE";
        case VAE_STATE_ENCODING:      return "ENCODING";
        case VAE_STATE_SAMPLING:      return "SAMPLING";
        case VAE_STATE_DECODING:      return "DECODING";
        case VAE_STATE_TRAINING:      return "TRAINING";
        case VAE_STATE_GENERATING:    return "GENERATING";
        case VAE_STATE_ERROR:         return "ERROR";
        case VAE_STATE_RECOVERING:    return "RECOVERING";
        default:                      return "UNKNOWN";
    }
}

int vae_get_stats(const vae_system_t* vae, vae_stats_t* stats)
{
    if (!vae || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_get_stats: required parameter is NULL (vae, stats)");
        return -1;
    }
    *stats = vae->stats;
    stats->last_update_us = get_timestamp_us();
    return 0;
}

int vae_get_health(const vae_system_t* vae, vae_health_t* health)
{
    if (!vae || !health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_get_health: required parameter is NULL (vae, health)");
        return -1;
    }
    *health = vae->health;
    return 0;
}

int vae_get_latent_state(const vae_system_t* vae, vae_latent_state_t* latent)
{
    if (!vae || !latent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_get_latent_state: required parameter is NULL (vae, latent)");
        return -1;
    }

    if (latent->latent_dim != vae->current_latent.latent_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_get_latent_state: validation failed");
        return -1;
    }

    if (latent->mu && vae->current_latent.mu) {
        memcpy(latent->mu, vae->current_latent.mu, latent->latent_dim * sizeof(float));
    }
    if (latent->log_var && vae->current_latent.log_var) {
        memcpy(latent->log_var, vae->current_latent.log_var, latent->latent_dim * sizeof(float));
    }
    if (latent->z && vae->current_latent.z) {
        memcpy(latent->z, vae->current_latent.z, latent->latent_dim * sizeof(float));
    }
    if (latent->precision && vae->current_latent.precision) {
        memcpy(latent->precision, vae->current_latent.precision, latent->latent_dim * sizeof(float));
    }

    latent->is_valid = vae->current_latent.is_valid;

    return 0;
}

int vae_get_config(const vae_system_t* vae, vae_config_t* config)
{
    if (!vae || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_get_config: required parameter is NULL (vae, config)");
        return -1;
    }
    *config = vae->config;
    return 0;
}

uint32_t vae_get_input_dim(const vae_system_t* vae)
{
    if (!vae) return 0;
    return vae->config.encoder.input_dim;
}

uint32_t vae_get_latent_dim(const vae_system_t* vae)
{
    if (!vae) return 0;
    return vae->config.encoder.latent_dim;
}

uint32_t vae_get_output_dim(const vae_system_t* vae)
{
    if (!vae) return 0;
    return vae->config.decoder.output_dim;
}

/* ============================================================================
 * Anomaly Detection API
 * ============================================================================ */

int vae_compute_anomaly_score(vae_system_t* vae,
                              const nimcp_tensor_t* input,
                              float* anomaly_score)
{
    if (!vae || !input || !anomaly_score) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_compute_anomaly_score: required parameter is NULL (vae, input, anomaly_score)");
        return -1;
    }

    uint32_t output_dim = vae->config.decoder.output_dim;
    uint32_t batch_size = (input->shape.rank >= 1) ? input->shape.dims[0] : 1;
    uint32_t output_dims[2] = {batch_size, output_dim};

    nimcp_tensor_t* recon = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_F32);
    if (!recon) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_compute_anomaly_score: recon is NULL");
        return -1;
    }

    /* Reconstruct */
    int result = vae_reconstruct(vae, input, recon);
    if (result != 0) {
        nimcp_tensor_destroy(recon);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_compute_anomaly_score: validation failed");
        return -1;
    }

    /* Compute MSE as anomaly score */
    *anomaly_score = vae_loss_mse(input, recon, VAE_LOSS_AGG_MEAN);

    nimcp_tensor_destroy(recon);

    return 0;
}

int vae_is_anomaly(vae_system_t* vae,
                   const nimcp_tensor_t* input,
                   bool* is_anomaly)
{
    if (!vae || !input || !is_anomaly) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_is_anomaly: required parameter is NULL (vae, input, is_anomaly)");
        return -1;
    }

    float score = 0.0f;
    int result = vae_compute_anomaly_score(vae, input, &score);
    if (result != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_is_anomaly: validation failed");
        return -1;
    }

    *is_anomaly = (score > vae->config.anomaly_threshold);

    if (*is_anomaly) {
        vae->stats.anomalies_detected++;
    }

    return 0;
}

/* ============================================================================
 * FEP Integration API
 * ============================================================================ */

float vae_get_free_energy(const vae_system_t* vae)
{
    if (!vae || !vae->is_initialized) return NAN;
    return vae->latest_loss.free_energy;
}

int vae_get_precision(const vae_system_t* vae, float* precision, uint32_t dim)
{
    if (!vae || !precision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_get_precision: required parameter is NULL (vae, precision)");
        return -1;
    }
    if (dim != vae->config.encoder.latent_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_get_precision: validation failed");
        return -1;
    }

    if (!vae->current_latent.precision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_get_precision: vae->current_latent is NULL");
        return -1;
    }

    memcpy(precision, vae->current_latent.precision, dim * sizeof(float));
    return 0;
}

float vae_get_avg_precision(const vae_system_t* vae)
{
    if (!vae || !vae->current_latent.log_var) return NAN;

    uint32_t dim = vae->current_latent.latent_dim;
    if (dim == 0) return NAN;

    /* Compute average precision directly from latent state log_var */
    float sum_precision = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float lv = vae->current_latent.log_var[i];
        if (lv < VAE_LATENT_MIN_LOG_VAR) lv = VAE_LATENT_MIN_LOG_VAR;
        if (lv > VAE_LATENT_MAX_LOG_VAR) lv = VAE_LATENT_MAX_LOG_VAR;
        sum_precision += expf(-lv);
    }

    return sum_precision / (float)dim;
}

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

void vae_set_health_agent(nimcp_health_agent_t* agent)
{
    g_vae_health_agent = agent;
    NIMCP_LOG_INFO("VAE: Global health agent set");
}

nimcp_health_agent_t* vae_get_health_agent(void)
{
    return g_vae_health_agent;
}

/* Latent state management functions are in nimcp_vae_latent.c */
