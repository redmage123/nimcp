/**
 * @file nimcp_vae_substrate_bridge.c
 * @brief VAE-Substrate Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Implements metabolic-aware VAE encoding with energy-efficient processing.
 *
 * BIO_MODULE: 0x1F1E
 */

#include "cognitive/vae/bridges/nimcp_vae_substrate_bridge.h"
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
#include "utils/math/nimcp_math_helpers.h"

/* ============================================================================
 * Module-level mutex for thread safety (VAE bridge has no bridge_base_t)
 * All public functions that read or write shared bridge state must lock/unlock.
 * ============================================================================ */
static nimcp_mutex_t* g_vae_substrate_mutex = NULL;

__attribute__((constructor))
static void vae_substrate_mutex_init(void) {
    g_vae_substrate_mutex = nimcp_mutex_create(NULL);
}

__attribute__((destructor))
static void vae_substrate_mutex_fini(void) {
    if (g_vae_substrate_mutex) {
        nimcp_mutex_destroy(g_vae_substrate_mutex);
        g_vae_substrate_mutex = NULL;
    }
}

static inline void vae_sub_lock(void) {
    if (g_vae_substrate_mutex) nimcp_mutex_lock(g_vae_substrate_mutex);
}

static inline void vae_sub_unlock(void) {
    if (g_vae_substrate_mutex) nimcp_mutex_unlock(g_vae_substrate_mutex);
}

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define VAE_SUB_MODULE_ID           BIO_MODULE_VAE_SUBSTRATE_BRIDGE
#define VAE_SUB_HISTORY_SIZE        32
#define VAE_SUB_EMA_ALPHA           0.9f
#define VAE_SUB_EMERGENCY_RECOVERY_US  5000000  /* 5 seconds */

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Q10 temperature scaling
 * rate_new = rate_base * Q10^((T - T_ref) / 10)
 */
static float q10_scale(float base_rate, float q10, float temp_c, float ref_temp_c)
{
    float delta = (temp_c - ref_temp_c) / 10.0f;
    return base_rate * powf(q10, delta);
}

/**
 * @brief Assess overall substrate health from metabolic state
 */
static vae_substrate_health_t assess_health_from_state(
    const vae_substrate_metabolic_state_t* state)
{
    if (!state) return VAE_SUBSTRATE_FAILURE;

    /* Check for critical conditions */
    if (state->atp_level < VAE_SUBSTRATE_CRITICAL_ATP ||
        state->o2_saturation < VAE_SUBSTRATE_CRITICAL_O2) {
        return VAE_SUBSTRATE_CRITICAL;
    }

    if (state->glucose_level < VAE_SUBSTRATE_CRITICAL_GLUCOSE ||
        state->ion_balance < VAE_SUBSTRATE_CRITICAL_ION) {
        return VAE_SUBSTRATE_CRITICAL;
    }

    /* Check for temperature extremes */
    if (state->temperature_c < VAE_SUBSTRATE_HYPOTHERMIA ||
        state->temperature_c > VAE_SUBSTRATE_HYPERTHERMIA) {
        return VAE_SUBSTRATE_CRITICAL;
    }

    /* Check for stressed conditions */
    if (state->atp_level < 0.5f || state->o2_saturation < 0.7f ||
        state->glucose_level < 0.6f || state->ion_balance < 0.7f) {
        return VAE_SUBSTRATE_STRESSED;
    }

    /* Check for minor deviations */
    if (state->atp_level < 0.8f || state->o2_saturation < 0.9f ||
        fabsf(state->temperature_c - VAE_SUBSTRATE_NORMAL_TEMP) > 1.0f) {
        return VAE_SUBSTRATE_NORMAL;
    }

    return VAE_SUBSTRATE_OPTIMAL;
}

/**
 * @brief Compute encoding modulation based on substrate state
 */
static float compute_encoding_modulation(const vae_substrate_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    const vae_substrate_metabolic_state_t* state = &bridge->current_state;

    /* ATP-based modulation: low ATP → slow down encoding */
    float atp_mod = nimcp_clampf(state->atp_level, 0.3f, 1.0f);

    /* O2-based modulation */
    float o2_mod = nimcp_clampf(state->o2_saturation, 0.5f, 1.0f);

    /* Temperature-based modulation via Q10 */
    float temp_mod = 1.0f;
    if (bridge->config.temperature.enable_temp_scaling) {
        temp_mod = q10_scale(1.0f,
                            bridge->config.temperature.q10_encoding,
                            state->temperature_c,
                            bridge->config.temperature.reference_temp_c);
        temp_mod = nimcp_clampf(temp_mod, 0.5f, 2.0f);
    }

    return atp_mod * o2_mod * temp_mod;
}

/**
 * @brief Compute effective latent dimension based on energy budget
 */
static uint32_t compute_effective_latent_dim(const vae_substrate_bridge_t* bridge)
{
    if (!bridge) return 32;

    const vae_substrate_metabolic_state_t* state = &bridge->current_state;
    const vae_substrate_compression_config_t* comp = &bridge->config.compression;

    /* Start with normal dimension */
    float dim_scale = 1.0f;

    /* Reduce dimension under ATP stress */
    if (state->atp_level < 0.5f) {
        dim_scale *= state->atp_level / 0.5f;
    }

    /* Further reduce in emergency mode */
    if (bridge->in_emergency_mode) {
        dim_scale *= 0.5f;
    }

    /* Apply compression rate */
    dim_scale *= (1.0f / (1.0f + comp->compression_rate *
                         (1.0f - state->atp_level)));

    uint32_t effective_dim = (uint32_t)(comp->normal_latent_dim * dim_scale);
    return (effective_dim < comp->min_latent_dim) ?
           comp->min_latent_dim : effective_dim;
}

/**
 * @brief Update history for trend detection
 */
static void update_history(vae_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->atp_history || !bridge->temp_history) return;

    bridge->atp_history[bridge->history_head] = bridge->current_state.atp_level;
    bridge->temp_history[bridge->history_head] = bridge->current_state.temperature_c;
    bridge->history_head = (bridge->history_head + 1) % bridge->history_size;
}

/**
 * @brief Update statistics
 */
static void update_stats(vae_substrate_bridge_t* bridge)
{
    if (!bridge) return;

    bridge->stats.total_operations++;

    /* Track ATP/temp extremes */
    if (bridge->current_state.atp_level < bridge->stats.min_atp_observed) {
        bridge->stats.min_atp_observed = bridge->current_state.atp_level;
    }
    if (bridge->current_state.temperature_c > bridge->stats.max_temp_observed) {
        bridge->stats.max_temp_observed = bridge->current_state.temperature_c;
    }
    if (bridge->current_state.temperature_c < bridge->stats.min_temp_observed) {
        bridge->stats.min_temp_observed = bridge->current_state.temperature_c;
    }

    /* Track time in emergency — guard against bogus delta on first call
     * when last_update_us is 0 (epoch-zero) */
    if (bridge->in_emergency_mode && bridge->stats.last_update_us > 0) {
        uint64_t now = get_timestamp_us();
        bridge->stats.time_in_emergency_us += now - bridge->stats.last_update_us;
    }

    bridge->stats.last_update_us = get_timestamp_us();
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_substrate_bridge_default_config(vae_substrate_bridge_config_t* config)
{
    if (!config) return NIMCP_ERROR_VAE_SUB_NULL;

    memset(config, 0, sizeof(*config));

    /* Adaptation strategy */
    config->adaptation_strategy = VAE_SUBSTRATE_ADAPT_HYBRID;

    /* Energy budget */
    config->energy_budget.max_atp_per_encode = 0.01f;
    config->energy_budget.max_atp_per_decode = 0.01f;
    config->energy_budget.max_atp_per_sample = 0.005f;
    config->energy_budget.atp_per_latent_dim = 0.0001f;
    config->energy_budget.reserve_fraction = 0.2f;

    /* Compression config */
    config->compression.min_latent_dim = 4;
    config->compression.normal_latent_dim = 32;
    config->compression.compression_rate = 0.5f;
    config->compression.enable_pruning = true;
    config->compression.pruning_threshold = 0.01f;

    /* Temperature config */
    config->temperature.q10_encoding = VAE_SUBSTRATE_Q10_ENCODING;
    config->temperature.q10_decoding = VAE_SUBSTRATE_Q10_ENCODING;
    config->temperature.q10_learning = VAE_SUBSTRATE_Q10_LEARNING;
    config->temperature.reference_temp_c = VAE_SUBSTRATE_NORMAL_TEMP;
    config->temperature.enable_temp_scaling = true;

    /* Monitoring */
    config->monitor_interval_ms = 100.0f;
    config->continuous_monitoring = true;

    /* Stress response */
    config->stress_onset_threshold = 0.6f;
    config->emergency_threshold = 0.3f;
    config->recovery_threshold = 0.7f;

    /* Uncertainty encoding */
    config->encode_uncertainty = true;
    config->stress_variance_scale = 2.0f;

    config->enable_logging = false;

    return 0;
}

vae_substrate_bridge_t* vae_substrate_bridge_create(const vae_substrate_bridge_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_substrate_bridge_create: config is NULL");
        return NULL;
    }

    vae_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_substrate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_substrate_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = VAE_SUBSTRATE_STATE_DISCONNECTED;
    bridge->is_initialized = false;
    bridge->creation_time_us = get_timestamp_us();

    /* Initialize current state to normal */
    bridge->current_state.atp_level = 1.0f;
    bridge->current_state.o2_saturation = 1.0f;
    bridge->current_state.glucose_level = 1.0f;
    bridge->current_state.ion_balance = 1.0f;
    bridge->current_state.temperature_c = VAE_SUBSTRATE_NORMAL_TEMP;
    bridge->current_state.timestamp_us = bridge->creation_time_us;

    bridge->current_health = VAE_SUBSTRATE_OPTIMAL;

    /* Allocate history */
    bridge->history_size = VAE_SUB_HISTORY_SIZE;
    bridge->atp_history = nimcp_calloc(VAE_SUB_HISTORY_SIZE, sizeof(float));
    bridge->temp_history = nimcp_calloc(VAE_SUB_HISTORY_SIZE, sizeof(float));

    if (!bridge->atp_history || !bridge->temp_history) {
        vae_substrate_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_substrate_bridge_create: required parameter is NULL (bridge->atp_history, bridge->temp_history)");
        return NULL;
    }

    /* Initialize history to normal values */
    for (uint32_t i = 0; i < VAE_SUB_HISTORY_SIZE; i++) {
        bridge->atp_history[i] = 1.0f;
        bridge->temp_history[i] = VAE_SUBSTRATE_NORMAL_TEMP;
    }

    /* Initialize modulation state */
    bridge->current_modulation.health = VAE_SUBSTRATE_OPTIMAL;
    bridge->current_modulation.encoding_modulation = 1.0f;
    bridge->current_modulation.learning_modulation = 1.0f;
    bridge->current_modulation.inference_modulation = 1.0f;
    bridge->current_modulation.effective_latent_dim = config->compression.normal_latent_dim;
    bridge->current_modulation.variance_scale = 1.0f;
    bridge->current_modulation.energy_efficiency = 1.0f;

    bridge->current_latent_dim = config->compression.normal_latent_dim;
    bridge->current_variance_scale = 1.0f;

    /* Initialize statistics */
    bridge->stats.min_atp_observed = 1.0f;
    bridge->stats.max_temp_observed = VAE_SUBSTRATE_NORMAL_TEMP;
    bridge->stats.min_temp_observed = VAE_SUBSTRATE_NORMAL_TEMP;
    bridge->stats.creation_time_us = bridge->creation_time_us;
    /* last_update_us starts at 0; update_stats guards against zero-delta emergency accounting */

    bridge->is_initialized = true;

    if (config->enable_logging) {
        nimcp_log_info(VAE_SUB_MODULE_ID, "VAE-Substrate Bridge created");
    }

    return bridge;
}

void vae_substrate_bridge_destroy(vae_substrate_bridge_t* bridge)
{
    if (!bridge) return;

    vae_substrate_bridge_disconnect(bridge);

    nimcp_free(bridge->atp_history);
    nimcp_free(bridge->temp_history);
    nimcp_free(bridge->modulation_buffer);

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_substrate_bridge_connect_vae(vae_substrate_bridge_t* bridge, vae_system_t* vae)
{
    if (!bridge) return NIMCP_ERROR_VAE_SUB_NULL;
    if (!vae) return NIMCP_ERROR_VAE_SUB_NULL;

    vae_sub_lock();
    bridge->vae = vae;

    /* Get VAE latent dimension for modulation buffer */
    uint32_t latent_dim = vae_get_latent_dim(vae);
    if (latent_dim > 0) {
        nimcp_free(bridge->modulation_buffer);
        bridge->modulation_buffer = nimcp_calloc(latent_dim, sizeof(float));
        if (!bridge->modulation_buffer) {
            vae_sub_unlock();
            return NIMCP_ERROR_VAE_SUB_NO_MEMORY;
        }

        /* Initialize to 1.0 (no modulation) */
        for (uint32_t i = 0; i < latent_dim; i++) {
            bridge->modulation_buffer[i] = 1.0f;
        }
    }

    if (bridge->substrate_system) {
        bridge->state = VAE_SUBSTRATE_STATE_CONNECTED;
    }
    vae_sub_unlock();

    if (bridge->config.enable_logging) {
        nimcp_log_info(VAE_SUB_MODULE_ID, "VAE connected (latent_dim=%u)", latent_dim);
    }

    return 0;
}

int vae_substrate_bridge_connect_substrate(vae_substrate_bridge_t* bridge, void* substrate)
{
    if (!bridge) return NIMCP_ERROR_VAE_SUB_NULL;

    vae_sub_lock();
    bridge->substrate_system = substrate;

    if (bridge->vae) {
        bridge->state = VAE_SUBSTRATE_STATE_CONNECTED;
    }
    vae_sub_unlock();

    if (bridge->config.enable_logging) {
        nimcp_log_info(VAE_SUB_MODULE_ID, "Substrate system connected");
    }

    return 0;
}

int vae_substrate_bridge_disconnect(vae_substrate_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_SUB_NULL;

    vae_sub_lock();
    bridge->vae = NULL;
    bridge->substrate_system = NULL;
    bridge->state = VAE_SUBSTRATE_STATE_DISCONNECTED;
    vae_sub_unlock();

    return 0;
}

bool vae_substrate_bridge_is_connected(const vae_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    vae_sub_lock();
    bool result = bridge->state == VAE_SUBSTRATE_STATE_CONNECTED ||
                  bridge->state == VAE_SUBSTRATE_STATE_MONITORING ||
                  bridge->state == VAE_SUBSTRATE_STATE_ADAPTING;
    vae_sub_unlock();
    return result;
}

/* ============================================================================
 * Monitoring API
 * ============================================================================ */

int vae_substrate_update_state(vae_substrate_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_SUB_NULL;

    vae_sub_lock();
    bridge->state = VAE_SUBSTRATE_STATE_MONITORING;

    /* In a full implementation, this would query the actual substrate system
     * for current metabolic state. For now, simulate with placeholder. */

    /* Update timestamp */
    bridge->current_state.timestamp_us = get_timestamp_us();

    /* Assess health */
    bridge->current_health = assess_health_from_state(&bridge->current_state);

    /* Update modulation values */
    bridge->current_modulation.health = bridge->current_health;
    bridge->current_modulation.encoding_modulation = compute_encoding_modulation(bridge);
    bridge->current_modulation.effective_latent_dim = compute_effective_latent_dim(bridge);

    /* Learning rate modulation via Q10 */
    if (bridge->config.temperature.enable_temp_scaling) {
        bridge->current_modulation.learning_modulation = q10_scale(1.0f,
            bridge->config.temperature.q10_learning,
            bridge->current_state.temperature_c,
            bridge->config.temperature.reference_temp_c);
        bridge->current_modulation.learning_modulation =
            nimcp_clampf(bridge->current_modulation.learning_modulation, 0.5f, 2.0f);
    }

    /* Inference modulation */
    bridge->current_modulation.inference_modulation = q10_scale(1.0f,
        VAE_SUBSTRATE_Q10_INFERENCE,
        bridge->current_state.temperature_c,
        VAE_SUBSTRATE_NORMAL_TEMP);

    /* Variance scaling under stress */
    if (bridge->config.encode_uncertainty) {
        if (bridge->current_health >= VAE_SUBSTRATE_STRESSED) {
            bridge->current_modulation.variance_scale =
                1.0f + (bridge->config.stress_variance_scale - 1.0f) *
                (1.0f - bridge->current_state.atp_level);
        } else {
            bridge->current_modulation.variance_scale = 1.0f;
        }
    }

    /* Energy efficiency */
    bridge->current_modulation.energy_efficiency =
        bridge->current_state.atp_level * bridge->current_state.glucose_level;

    /* Update history */
    update_history(bridge);

    /* Check for emergency transitions */
    bool do_enter_emergency = (bridge->current_health == VAE_SUBSTRATE_CRITICAL && !bridge->in_emergency_mode);
    bool do_exit_emergency = bridge->in_emergency_mode &&
        bridge->current_state.atp_level > bridge->config.recovery_threshold &&
        bridge->current_state.o2_saturation > bridge->config.recovery_threshold;

    update_stats(bridge);

    bridge->state = VAE_SUBSTRATE_STATE_CONNECTED;
    vae_sub_unlock();

    /* Call emergency transitions outside the lock to avoid callback-under-lock */
    if (do_enter_emergency) {
        vae_substrate_enter_emergency(bridge);
    } else if (do_exit_emergency) {
        vae_substrate_exit_emergency(bridge);
    }

    return 0;
}

int vae_substrate_get_metabolic_state(const vae_substrate_bridge_t* bridge,
                                       vae_substrate_metabolic_state_t* state)
{
    if (!bridge || !state) return NIMCP_ERROR_VAE_SUB_NULL;
    vae_sub_lock();
    *state = bridge->current_state;
    vae_sub_unlock();
    return 0;
}

vae_substrate_health_t vae_substrate_assess_health(const vae_substrate_bridge_t* bridge)
{
    if (!bridge) return VAE_SUBSTRATE_FAILURE;
    vae_sub_lock();
    vae_substrate_health_t h = bridge->current_health;
    vae_sub_unlock();
    return h;
}

int vae_substrate_get_modulation(const vae_substrate_bridge_t* bridge,
                                  vae_substrate_modulation_t* modulation)
{
    if (!bridge || !modulation) return NIMCP_ERROR_VAE_SUB_NULL;
    vae_sub_lock();
    *modulation = bridge->current_modulation;
    vae_sub_unlock();
    return 0;
}

/* ============================================================================
 * Adaptation API
 * ============================================================================ */

int vae_substrate_adapt(vae_substrate_bridge_t* bridge,
                         vae_substrate_adaptation_result_t* result)
{
    if (!bridge || !result) return NIMCP_ERROR_VAE_SUB_NULL;

    vae_sub_lock();
    uint64_t start_us = get_timestamp_us();
    bridge->state = VAE_SUBSTRATE_STATE_ADAPTING;

    memset(result, 0, sizeof(*result));
    result->strategy_used = bridge->config.adaptation_strategy;
    result->latent_dim_before = bridge->current_latent_dim;

    /* Compute new latent dimension */
    uint32_t new_dim = compute_effective_latent_dim(bridge);

    switch (bridge->config.adaptation_strategy) {
        case VAE_SUBSTRATE_ADAPT_COMPRESS:
            /* Simply reduce latent dimensions */
            bridge->current_latent_dim = new_dim;
            break;

        case VAE_SUBSTRATE_ADAPT_SPARSE:
            /* Keep dimensions but increase sparsity */
            /* Would modify encoding to produce sparser representations */
            bridge->current_latent_dim = new_dim;
            break;

        case VAE_SUBSTRATE_ADAPT_QUANTIZE:
            /* Quantize latent values to reduce precision */
            /* Would reduce bits per latent value */
            bridge->current_latent_dim = new_dim;
            break;

        case VAE_SUBSTRATE_ADAPT_THROTTLE:
            /* Reduce update frequency */
            /* Would skip some encoding operations */
            break;

        case VAE_SUBSTRATE_ADAPT_HYBRID:
        default:
            /* Combine strategies based on severity */
            if (bridge->current_health == VAE_SUBSTRATE_CRITICAL) {
                /* Maximum compression */
                bridge->current_latent_dim = bridge->config.compression.min_latent_dim;
            } else if (bridge->current_health == VAE_SUBSTRATE_STRESSED) {
                /* Moderate compression */
                bridge->current_latent_dim = new_dim;
            } else {
                /* Normal operation */
                bridge->current_latent_dim = bridge->config.compression.normal_latent_dim;
            }
            break;
    }

    result->latent_dim_after = bridge->current_latent_dim;
    /* Guard: prevent division by zero if latent_dim_after is 0 (e.g. min_latent_dim=0) */
    if (result->latent_dim_after == 0) result->latent_dim_after = 1;
    result->compression_ratio = (float)result->latent_dim_before /
                               (float)result->latent_dim_after;

    /* Estimate quality loss */
    if (result->latent_dim_after < result->latent_dim_before) {
        result->quality_loss_estimate = 1.0f - sqrtf((float)result->latent_dim_after /
                                                     (float)result->latent_dim_before);
    } else {
        result->quality_loss_estimate = 0.0f;
    }

    result->adaptation_time_us = get_timestamp_us() - start_us;

    bridge->stats.adaptations_triggered++;
    bridge->state = VAE_SUBSTRATE_STATE_CONNECTED;
    vae_sub_unlock();

    if (bridge->config.enable_logging) {
        nimcp_log_info(VAE_SUB_MODULE_ID,
                      "Adapted: dim %u -> %u, ratio %.2f",
                      result->latent_dim_before, result->latent_dim_after,
                      result->compression_ratio);
    }

    return 0;
}

int vae_substrate_enter_emergency(vae_substrate_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_SUB_NULL;

    vae_sub_lock();
    if (bridge->in_emergency_mode) { vae_sub_unlock(); return 0; } /* Already in emergency */

    bridge->in_emergency_mode = true;
    bridge->emergency_start_us = get_timestamp_us();
    bridge->state = VAE_SUBSTRATE_STATE_EMERGENCY;
    bridge->stats.emergency_events++;

    /* Force minimum latent dimension */
    bridge->current_latent_dim = bridge->config.compression.min_latent_dim;

    /* Increase variance for uncertainty */
    bridge->current_variance_scale = bridge->config.stress_variance_scale;

    float atp_snap = bridge->current_state.atp_level;
    float o2_snap = bridge->current_state.o2_saturation;
    vae_sub_unlock();

    if (bridge->config.enable_logging) {
        nimcp_log_warning(VAE_SUB_MODULE_ID,
                         "EMERGENCY MODE ENTERED - ATP=%.2f, O2=%.2f",
                         atp_snap,
                         o2_snap);
    }

    return 0;
}

int vae_substrate_exit_emergency(vae_substrate_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_SUB_NULL;

    vae_sub_lock();
    if (!bridge->in_emergency_mode) { vae_sub_unlock(); return 0; }

    /* Ensure sufficient recovery time */
    uint64_t now = get_timestamp_us();
    if (now - bridge->emergency_start_us < VAE_SUB_EMERGENCY_RECOVERY_US) {
        vae_sub_unlock();
        return 0; /* Not enough time for stable recovery */
    }

    bridge->in_emergency_mode = false;
    bridge->state = VAE_SUBSTRATE_STATE_CONNECTED;

    /* Restore normal latent dimension */
    bridge->current_latent_dim = bridge->config.compression.normal_latent_dim;
    bridge->current_variance_scale = 1.0f;
    vae_sub_unlock();

    if (bridge->config.enable_logging) {
        nimcp_log_info(VAE_SUB_MODULE_ID,
                      "Emergency mode exited - recovered");
    }

    return 0;
}

int vae_substrate_set_latent_dim(vae_substrate_bridge_t* bridge, uint32_t dim)
{
    if (!bridge) return NIMCP_ERROR_VAE_SUB_NULL;

    vae_sub_lock();
    dim = (uint32_t)nimcp_clampf((float)dim,
                                 (float)bridge->config.compression.min_latent_dim,
                                 (float)bridge->config.compression.normal_latent_dim);
    bridge->current_latent_dim = dim;
    vae_sub_unlock();

    return 0;
}

/* ============================================================================
 * Energy API
 * ============================================================================ */

float vae_substrate_estimate_cost(const vae_substrate_bridge_t* bridge,
                                   vae_energy_category_t category)
{
    if (!bridge) return 0.0f;

    const vae_substrate_energy_budget_t* budget = &bridge->config.energy_budget;

    float base_cost = 0.0f;
    switch (category) {
        case VAE_ENERGY_ENCODE:
            base_cost = budget->max_atp_per_encode;
            break;
        case VAE_ENERGY_DECODE:
            base_cost = budget->max_atp_per_decode;
            break;
        case VAE_ENERGY_SAMPLE:
            base_cost = budget->max_atp_per_sample;
            break;
        case VAE_ENERGY_TRAIN:
            base_cost = budget->max_atp_per_encode * 2.0f +
                       budget->max_atp_per_decode;
            break;
        case VAE_ENERGY_TOTAL:
        default:
            base_cost = budget->max_atp_per_encode +
                       budget->max_atp_per_decode +
                       budget->max_atp_per_sample;
            break;
    }

    vae_sub_lock();
    /* Guard: prevent divide-by-zero if normal_latent_dim is 0 */
    float dim_scale = (bridge->config.compression.normal_latent_dim > 0)
        ? (float)bridge->current_latent_dim / (float)bridge->config.compression.normal_latent_dim
        : 1.0f;
    vae_sub_unlock();

    return base_cost * dim_scale;
}

bool vae_substrate_can_afford(const vae_substrate_bridge_t* bridge,
                               vae_energy_category_t category)
{
    if (!bridge) {
        return false;
    }

    float cost = vae_substrate_estimate_cost(bridge, category);
    vae_sub_lock();
    float available = bridge->current_state.atp_level *
                     (1.0f - bridge->config.energy_budget.reserve_fraction);
    vae_sub_unlock();

    return cost <= available;
}

int vae_substrate_consume_energy(vae_substrate_bridge_t* bridge,
                                  vae_energy_category_t category,
                                  vae_substrate_energy_result_t* result)
{
    if (!bridge || !result) return NIMCP_ERROR_VAE_SUB_NULL;

    uint64_t start_us = get_timestamp_us();
    memset(result, 0, sizeof(*result));

    float cost = vae_substrate_estimate_cost(bridge, category);

    /* Check if affordable */
    if (!vae_substrate_can_afford(bridge, category)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_SUB_CRITICAL, "vae_substrate_bridge: error condition");
        return NIMCP_ERROR_VAE_SUB_CRITICAL;
    }

    /* Consume energy — lock for shared state mutation */
    vae_sub_lock();
    bridge->atp_consumed_session += cost;
    bridge->glucose_consumed_session += cost * 0.5f; /* Glucose -> ATP conversion */

    /* Update internal state (would normally update actual substrate) */
    bridge->current_state.atp_level -= cost;
    bridge->current_state.atp_level = nimcp_clampf(bridge->current_state.atp_level, 0.0f, 1.0f);

    result->atp_used = cost;
    result->atp_remaining = bridge->current_state.atp_level;
    result->glucose_consumed = cost * 0.5f;
    result->efficiency_ratio = 1.0f / (1.0f + cost * 10.0f);
    result->operation_time_us = get_timestamp_us() - start_us;

    /* Update total ATP consumed */
    bridge->stats.total_atp_consumed += cost;
    vae_sub_unlock();

    return 0;
}

float vae_substrate_get_efficiency(const vae_substrate_bridge_t* bridge)
{
    if (!bridge) return 0.0f;
    vae_sub_lock();
    float eff = bridge->current_modulation.energy_efficiency;
    vae_sub_unlock();
    return eff;
}

/* ============================================================================
 * Temperature API
 * ============================================================================ */

float vae_substrate_q10_scale(const vae_substrate_bridge_t* bridge,
                               float base_rate,
                               float q10_coefficient)
{
    if (!bridge) return base_rate;

    vae_sub_lock();
    float temp_c = bridge->current_state.temperature_c;
    float ref_c = bridge->config.temperature.reference_temp_c;
    vae_sub_unlock();

    return q10_scale(base_rate, q10_coefficient, temp_c, ref_c);
}

float vae_substrate_get_temp_modulated_lr(const vae_substrate_bridge_t* bridge,
                                           float base_lr)
{
    if (!bridge) return base_lr;

    if (!bridge->config.temperature.enable_temp_scaling) {
        return base_lr;
    }

    return vae_substrate_q10_scale(bridge, base_lr,
                                   bridge->config.temperature.q10_learning);
}

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_substrate_bridge_state_t vae_substrate_bridge_get_state(const vae_substrate_bridge_t* bridge)
{
    if (!bridge) return VAE_SUBSTRATE_STATE_ERROR;
    vae_sub_lock();
    vae_substrate_bridge_state_t s = bridge->state;
    vae_sub_unlock();
    return s;
}

int vae_substrate_bridge_get_stats(const vae_substrate_bridge_t* bridge,
                                    vae_substrate_bridge_stats_t* stats)
{
    if (!bridge || !stats) return NIMCP_ERROR_VAE_SUB_NULL;
    vae_sub_lock();
    *stats = bridge->stats;
    vae_sub_unlock();
    return 0;
}

const char* vae_substrate_health_to_string(vae_substrate_health_t health)
{
    switch (health) {
        case VAE_SUBSTRATE_OPTIMAL: return "optimal";
        case VAE_SUBSTRATE_NORMAL: return "normal";
        case VAE_SUBSTRATE_STRESSED: return "stressed";
        case VAE_SUBSTRATE_CRITICAL: return "critical";
        case VAE_SUBSTRATE_FAILURE: return "failure";
        default: return "unknown";
    }
}

const char* vae_substrate_adaptation_to_string(vae_substrate_adaptation_t adapt)
{
    switch (adapt) {
        case VAE_SUBSTRATE_ADAPT_COMPRESS: return "compress";
        case VAE_SUBSTRATE_ADAPT_SPARSE: return "sparse";
        case VAE_SUBSTRATE_ADAPT_QUANTIZE: return "quantize";
        case VAE_SUBSTRATE_ADAPT_THROTTLE: return "throttle";
        case VAE_SUBSTRATE_ADAPT_HYBRID: return "hybrid";
        default: return "unknown";
    }
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_substrate_energy_result_free(vae_substrate_energy_result_t* result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));
}

void vae_substrate_adaptation_result_free(vae_substrate_adaptation_result_t* result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));
}
