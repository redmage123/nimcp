/**
 * @file nimcp_vae_bbb_bridge.c
 * @brief VAE-BBB Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Implements security integration between VAE and Blood-Brain Barrier.
 *
 * BIO_MODULE: 0x1F12
 */

#include "cognitive/vae/bridges/nimcp_vae_bbb_bridge.h"
#include "cognitive/vae/nimcp_vae.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
/* TODO: Fix immune path #include "immune/nimcp_immune.h" */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define VAE_BBB_MODULE_ID               BIO_MODULE_VAE_BBB_BRIDGE
#define VAE_BBB_EMA_ALPHA               0.95f
#define VAE_BBB_MAX_CONSEC_BLOCKS       10

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Check if float is valid (not NaN or Inf)
 */
static inline bool is_valid_float(float val)
{
    return !isnan(val) && !isinf(val);
}

/**
 * @brief Count invalid values in tensor
 */
static uint32_t count_invalid_values(const nimcp_tensor_t* tensor)
{
    if (!tensor || !tensor->data) return 0;

    uint32_t count = 0;
    uint32_t total = nimcp_tensor_numel(tensor);
    const float* data = (const float*)tensor->data;

    for (uint32_t i = 0; i < total; i++) {
        if (!is_valid_float(data[i])) {
            count++;
        }
    }

    return count;
}

/**
 * @brief Count out-of-bounds values
 */
static uint32_t count_out_of_bounds(const nimcp_tensor_t* tensor,
                                    float min_val, float max_val)
{
    if (!tensor || !tensor->data) return 0;

    uint32_t count = 0;
    uint32_t total = nimcp_tensor_numel(tensor);
    const float* data = (const float*)tensor->data;

    for (uint32_t i = 0; i < total; i++) {
        if (data[i] < min_val || data[i] > max_val) {
            count++;
        }
    }

    return count;
}

/**
 * @brief Get max magnitude in tensor
 */
static float get_max_magnitude(const nimcp_tensor_t* tensor)
{
    if (!tensor || !tensor->data) return 0.0f;

    float max_mag = 0.0f;
    uint32_t total = nimcp_tensor_numel(tensor);
    const float* data = (const float*)tensor->data;

    for (uint32_t i = 0; i < total; i++) {
        float mag = fabsf(data[i]);
        if (is_valid_float(mag) && mag > max_mag) {
            max_mag = mag;
        }
    }

    return max_mag;
}

/**
 * @brief Update health metrics
 */
static void update_health(vae_bbb_bridge_t* bridge, vae_bbb_result_t result)
{
    if (!bridge) return;

    if (result == VAE_BBB_RESULT_PASS || result == VAE_BBB_RESULT_SANITIZED) {
        bridge->health.consecutive_blocks = 0;
        bridge->health.bridge_health = fminf(1.0f,
            bridge->health.bridge_health + 0.01f);
    } else if (result == VAE_BBB_RESULT_BLOCKED) {
        bridge->health.consecutive_blocks++;
        bridge->health.bridge_health = fmaxf(0.1f,
            bridge->health.bridge_health - 0.05f);

        if (bridge->health.consecutive_blocks >= VAE_BBB_MAX_CONSEC_BLOCKS) {
            bridge->health.is_healthy = false;
        }
    }

    /* Update rates */
    if (bridge->stats.total_validations > 0) {
        bridge->health.pass_rate = (float)(bridge->stats.passed + bridge->stats.sanitized) /
                                   (float)bridge->stats.total_validations;
        bridge->health.threat_rate = (float)bridge->stats.blocked /
                                     (float)bridge->stats.total_validations;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_bbb_bridge_default_config(vae_bbb_bridge_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_BBB_MODULE_ID, NIMCP_ERROR_VAE_BBB_NULL_BRIDGE,
                             "NULL config in vae_bbb_bridge_default_config");
        return -1;
    }

    memset(config, 0, sizeof(vae_bbb_bridge_config_t));

    /* Validation settings */
    config->validation_mask = VAE_BBB_VALIDATE_ALL;
    config->strict_mode = false;

    /* Bounds */
    config->bounds.input_min = -10.0f;
    config->bounds.input_max = 10.0f;
    config->bounds.latent_max = VAE_BBB_DEFAULT_LATENT_MAX;
    config->bounds.output_min = 0.0f;
    config->bounds.output_max = VAE_BBB_DEFAULT_OUTPUT_MAX;
    config->bounds.gradient_max = VAE_BBB_DEFAULT_GRADIENT_MAX;

    /* Sanitization */
    config->enable_sanitization = true;
    config->clamp_values = true;
    config->replace_nan = true;
    config->nan_replacement = 0.0f;

    /* Adversarial detection */
    config->detect_adversarial = true;
    config->adversarial_threshold = 0.1f;
    config->block_adversarial = false;  /* Report but don't block by default */

    /* Actions */
    config->report_to_bbb = true;
    config->quarantine_threats = false;
    config->log_validations = false;

    /* Performance */
    config->async_validation = false;
    config->batch_size = 32;

    return 0;
}

vae_bbb_bridge_t* vae_bbb_bridge_create(const vae_bbb_bridge_config_t* config)
{
    vae_bbb_bridge_config_t default_config;

    if (!config) {
        if (vae_bbb_bridge_default_config(&default_config) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_bridge_create: validation failed");
            return NULL;
        }
        config = &default_config;
    }

    vae_bbb_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_bbb_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_BBB_MODULE_ID, NIMCP_ERROR_VAE_NO_MEMORY,
                             "Failed to allocate VAE-BBB bridge");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = VAE_BBB_STATE_DISCONNECTED;
    bridge->vae = NULL;
    bridge->bbb = NULL;

    /* Create mutex */
    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    bridge->mutex = nimcp_mutex_create(&attr);
    if (!bridge->mutex) {
        NIMCP_LOG_ERROR("VAE-BBB Bridge: Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_bbb_bridge_create: bridge->mutex is NULL");
        return NULL;
    }

    /* Initialize health */
    bridge->health.bridge_health = 1.0f;
    bridge->health.is_healthy = true;
    bridge->health.pass_rate = 1.0f;
    bridge->health.threat_rate = 0.0f;

    /* Initialize timing */
    bridge->creation_time_us = get_timestamp_us();

    bridge->is_initialized = true;

    NIMCP_LOG_INFO("VAE-BBB Bridge: Created");

    return bridge;
}

void vae_bbb_bridge_destroy(vae_bbb_bridge_t* bridge)
{
    if (!bridge) return;

    NIMCP_LOG_INFO("VAE-BBB Bridge: Destroying");

    vae_bbb_bridge_disconnect(bridge);

    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_bbb_bridge_reset(vae_bbb_bridge_t* bridge)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_BBB_MODULE_ID, NIMCP_ERROR_VAE_BBB_NULL_BRIDGE,
                             "Invalid bridge in reset");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(vae_bbb_bridge_stats_t));

    /* Reset health */
    bridge->health.bridge_health = 1.0f;
    bridge->health.is_healthy = true;
    bridge->health.pass_rate = 1.0f;
    bridge->health.threat_rate = 0.0f;
    bridge->health.consecutive_blocks = 0;

    /* Reset last validation */
    memset(&bridge->last_validation, 0, sizeof(vae_bbb_validation_t));

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_DEBUG("VAE-BBB Bridge: Reset");

    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int vae_bbb_bridge_connect_vae(vae_bbb_bridge_t* bridge, vae_system_t* vae)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_BBB_MODULE_ID, NIMCP_ERROR_VAE_BBB_NULL_BRIDGE,
                             "Invalid bridge in connect_vae");
        return -1;
    }

    if (!vae) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_BBB_MODULE_ID, NIMCP_ERROR_VAE_BBB_NO_VAE,
                             "NULL VAE in connect_vae");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->vae = vae;

    if (bridge->bbb) {
        bridge->state = VAE_BBB_STATE_CONNECTED;
    }

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_INFO("VAE-BBB Bridge: VAE connected");

    return 0;
}

int vae_bbb_bridge_connect_bbb(vae_bbb_bridge_t* bridge, bbb_system_t bbb)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_BBB_MODULE_ID, NIMCP_ERROR_VAE_BBB_NULL_BRIDGE,
                             "Invalid bridge in connect_bbb");
        return -1;
    }

    if (!bbb) {
        NIMCP_THROW_TO_IMMUNE_MODULE(VAE_BBB_MODULE_ID, NIMCP_ERROR_VAE_BBB_NO_BBB,
                             "NULL BBB in connect_bbb");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->bbb = bbb;

    if (bridge->vae) {
        bridge->state = VAE_BBB_STATE_CONNECTED;
    }

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_INFO("VAE-BBB Bridge: BBB connected");

    return 0;
}

int vae_bbb_bridge_disconnect(vae_bbb_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_bridge_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->vae = NULL;
    bridge->bbb = NULL;
    bridge->state = VAE_BBB_STATE_DISCONNECTED;

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_INFO("VAE-BBB Bridge: Disconnected");

    return 0;
}

bool vae_bbb_bridge_is_connected(const vae_bbb_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return (bridge->vae != NULL) && (bridge->bbb != NULL);
}

/* ============================================================================
 * Validation API
 * ============================================================================ */

vae_bbb_result_t vae_bbb_validate_input(vae_bbb_bridge_t* bridge,
                                         const nimcp_tensor_t* input,
                                         vae_bbb_validation_t* result)
{
    if (!bridge || !input) {
        if (result) {
            result->result = VAE_BBB_RESULT_ERROR;
            snprintf(result->reason, sizeof(result->reason), "NULL bridge or input");
        }
        return VAE_BBB_RESULT_ERROR;
    }

    if (!(bridge->config.validation_mask & VAE_BBB_VALIDATE_INPUT)) {
        if (result) {
            result->result = VAE_BBB_RESULT_PASS;
        }
        return VAE_BBB_RESULT_PASS;
    }

    nimcp_mutex_lock(bridge->mutex);

    uint64_t start_time = get_timestamp_us();
    bridge->state = VAE_BBB_STATE_VALIDATING;
    bridge->stats.total_validations++;
    bridge->stats.input_validations++;

    vae_bbb_validation_t validation = {0};
    validation.stage = VAE_BBB_STAGE_INPUT;
    validation.result = VAE_BBB_RESULT_PASS;

    /* Check for NaN/Inf */
    uint32_t invalid_count = count_invalid_values(input);
    if (invalid_count > 0) {
        validation.threat = VAE_THREAT_NAN_INF;
        validation.violations_found += invalid_count;
        validation.severity = BBB_SEVERITY_HIGH;

        if (bridge->config.replace_nan) {
            validation.result = VAE_BBB_RESULT_SANITIZED;
            validation.sanitized = true;
        } else {
            validation.result = VAE_BBB_RESULT_BLOCKED;
            snprintf(validation.reason, sizeof(validation.reason),
                     "Found %u NaN/Inf values", invalid_count);
        }

        bridge->stats.nan_inf_detected++;
    }

    /* Check bounds */
    if (validation.result != VAE_BBB_RESULT_BLOCKED) {
        uint32_t oob_count = count_out_of_bounds(input,
            bridge->config.bounds.input_min,
            bridge->config.bounds.input_max);

        if (oob_count > 0) {
            validation.threat = VAE_THREAT_OUT_OF_BOUNDS;
            validation.violations_found += oob_count;

            if (bridge->config.clamp_values) {
                if (validation.result == VAE_BBB_RESULT_PASS) {
                    validation.result = VAE_BBB_RESULT_SANITIZED;
                }
                validation.sanitized = true;
            } else if (bridge->config.strict_mode) {
                validation.result = VAE_BBB_RESULT_BLOCKED;
                snprintf(validation.reason, sizeof(validation.reason),
                         "Found %u out-of-bounds values", oob_count);
            }

            bridge->stats.out_of_bounds++;
        }
    }

    /* Check tensor structure */
    if (validation.result != VAE_BBB_RESULT_BLOCKED) {
        if (!input->data || nimcp_tensor_numel(input) == 0) {
            validation.threat = VAE_THREAT_MALFORMED_TENSOR;
            validation.result = VAE_BBB_RESULT_BLOCKED;
            validation.severity = BBB_SEVERITY_MEDIUM;
            snprintf(validation.reason, sizeof(validation.reason),
                     "Malformed tensor (null or empty)");
            bridge->stats.malformed_tensors++;
        }
    }

    /* Update statistics */
    uint64_t elapsed = get_timestamp_us() - start_time;
    float alpha = VAE_BBB_EMA_ALPHA;
    bridge->stats.avg_validation_time_us = alpha * bridge->stats.avg_validation_time_us +
                                           (1.0f - alpha) * (float)elapsed;
    if (elapsed > bridge->stats.max_validation_time_us) {
        bridge->stats.max_validation_time_us = (float)elapsed;
    }

    switch (validation.result) {
        case VAE_BBB_RESULT_PASS:
            bridge->stats.passed++;
            break;
        case VAE_BBB_RESULT_SANITIZED:
            bridge->stats.sanitized++;
            break;
        case VAE_BBB_RESULT_BLOCKED:
            bridge->stats.blocked++;
            bridge->state = VAE_BBB_STATE_BLOCKING;
            break;
        default:
            break;
    }

    /* Report to BBB if configured */
    if (validation.result == VAE_BBB_RESULT_BLOCKED && bridge->config.report_to_bbb) {
        vae_bbb_report_threat(bridge, validation.threat, validation.severity,
                              input->data, nimcp_tensor_numel(input) * sizeof(float));
    }

    bridge->last_validation = validation;
    update_health(bridge, validation.result);

    if (bridge->state != VAE_BBB_STATE_BLOCKING) {
        bridge->state = VAE_BBB_STATE_CONNECTED;
    }

    nimcp_mutex_unlock(bridge->mutex);

    if (result) {
        *result = validation;
    }

    if (bridge->config.log_validations) {
        NIMCP_LOG_DEBUG("VAE-BBB: Input validation %s (violations=%u)",
                        vae_bbb_result_to_string(validation.result),
                        validation.violations_found);
    }

    return validation.result;
}

vae_bbb_result_t vae_bbb_validate_latent(vae_bbb_bridge_t* bridge,
                                          const nimcp_tensor_t* latent,
                                          vae_bbb_validation_t* result)
{
    if (!bridge || !latent) {
        if (result) {
            result->result = VAE_BBB_RESULT_ERROR;
        }
        return VAE_BBB_RESULT_ERROR;
    }

    if (!(bridge->config.validation_mask & VAE_BBB_VALIDATE_LATENT)) {
        if (result) {
            result->result = VAE_BBB_RESULT_PASS;
        }
        return VAE_BBB_RESULT_PASS;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->stats.total_validations++;
    bridge->stats.latent_validations++;

    vae_bbb_validation_t validation = {0};
    validation.stage = VAE_BBB_STAGE_LATENT;
    validation.result = VAE_BBB_RESULT_PASS;

    /* Check for NaN/Inf */
    uint32_t invalid_count = count_invalid_values(latent);
    if (invalid_count > 0) {
        validation.threat = VAE_THREAT_NAN_INF;
        validation.violations_found += invalid_count;
        validation.result = VAE_BBB_RESULT_BLOCKED;
        validation.severity = BBB_SEVERITY_CRITICAL;
        snprintf(validation.reason, sizeof(validation.reason),
                 "NaN/Inf in latent space");
        bridge->stats.nan_inf_detected++;
    }

    /* Check magnitude bounds */
    if (validation.result != VAE_BBB_RESULT_BLOCKED) {
        float max_mag = get_max_magnitude(latent);
        if (max_mag > bridge->config.bounds.latent_max) {
            validation.threat = VAE_THREAT_OUT_OF_BOUNDS;
            validation.violations_found++;
            validation.severity = BBB_SEVERITY_HIGH;

            if (bridge->config.clamp_values) {
                validation.result = VAE_BBB_RESULT_SANITIZED;
                validation.sanitized = true;
            } else {
                validation.result = VAE_BBB_RESULT_BLOCKED;
                snprintf(validation.reason, sizeof(validation.reason),
                         "Latent magnitude %.2f exceeds max %.2f",
                         max_mag, bridge->config.bounds.latent_max);
            }
            bridge->stats.out_of_bounds++;
        }
    }

    /* Update statistics */
    switch (validation.result) {
        case VAE_BBB_RESULT_PASS:
            bridge->stats.passed++;
            break;
        case VAE_BBB_RESULT_SANITIZED:
            bridge->stats.sanitized++;
            break;
        case VAE_BBB_RESULT_BLOCKED:
            bridge->stats.blocked++;
            break;
        default:
            break;
    }

    bridge->last_validation = validation;
    update_health(bridge, validation.result);

    nimcp_mutex_unlock(bridge->mutex);

    if (result) {
        *result = validation;
    }

    return validation.result;
}

vae_bbb_result_t vae_bbb_validate_output(vae_bbb_bridge_t* bridge,
                                          const nimcp_tensor_t* output,
                                          vae_bbb_validation_t* result)
{
    if (!bridge || !output) {
        if (result) {
            result->result = VAE_BBB_RESULT_ERROR;
        }
        return VAE_BBB_RESULT_ERROR;
    }

    if (!(bridge->config.validation_mask & VAE_BBB_VALIDATE_OUTPUT)) {
        if (result) {
            result->result = VAE_BBB_RESULT_PASS;
        }
        return VAE_BBB_RESULT_PASS;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->stats.total_validations++;
    bridge->stats.output_validations++;

    vae_bbb_validation_t validation = {0};
    validation.stage = VAE_BBB_STAGE_OUTPUT;
    validation.result = VAE_BBB_RESULT_PASS;

    /* Check for NaN/Inf */
    uint32_t invalid_count = count_invalid_values(output);
    if (invalid_count > 0) {
        validation.threat = VAE_THREAT_NAN_INF;
        validation.violations_found += invalid_count;

        if (bridge->config.replace_nan) {
            validation.result = VAE_BBB_RESULT_SANITIZED;
            validation.sanitized = true;
        } else {
            validation.result = VAE_BBB_RESULT_BLOCKED;
            validation.severity = BBB_SEVERITY_HIGH;
        }
        bridge->stats.nan_inf_detected++;
    }

    /* Check output bounds */
    if (validation.result != VAE_BBB_RESULT_BLOCKED) {
        uint32_t oob_count = count_out_of_bounds(output,
            bridge->config.bounds.output_min,
            bridge->config.bounds.output_max);

        if (oob_count > 0) {
            validation.threat = VAE_THREAT_OUT_OF_BOUNDS;
            validation.violations_found += oob_count;

            if (bridge->config.clamp_values) {
                if (validation.result == VAE_BBB_RESULT_PASS) {
                    validation.result = VAE_BBB_RESULT_SANITIZED;
                }
                validation.sanitized = true;
            }
            bridge->stats.out_of_bounds++;
        }
    }

    switch (validation.result) {
        case VAE_BBB_RESULT_PASS:
            bridge->stats.passed++;
            break;
        case VAE_BBB_RESULT_SANITIZED:
            bridge->stats.sanitized++;
            break;
        case VAE_BBB_RESULT_BLOCKED:
            bridge->stats.blocked++;
            break;
        default:
            break;
    }

    bridge->last_validation = validation;
    update_health(bridge, validation.result);

    nimcp_mutex_unlock(bridge->mutex);

    if (result) {
        *result = validation;
    }

    return validation.result;
}

vae_bbb_result_t vae_bbb_validate_gradients(vae_bbb_bridge_t* bridge,
                                             const nimcp_tensor_t* gradients,
                                             vae_bbb_validation_t* result)
{
    if (!bridge || !gradients) {
        if (result) {
            result->result = VAE_BBB_RESULT_ERROR;
        }
        return VAE_BBB_RESULT_ERROR;
    }

    if (!(bridge->config.validation_mask & VAE_BBB_VALIDATE_GRADIENT)) {
        if (result) {
            result->result = VAE_BBB_RESULT_PASS;
        }
        return VAE_BBB_RESULT_PASS;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->stats.total_validations++;
    bridge->stats.gradient_validations++;

    vae_bbb_validation_t validation = {0};
    validation.stage = VAE_BBB_STAGE_GRADIENT;
    validation.result = VAE_BBB_RESULT_PASS;

    /* Check for NaN/Inf in gradients */
    uint32_t invalid_count = count_invalid_values(gradients);
    if (invalid_count > 0) {
        validation.threat = VAE_THREAT_GRADIENT_ATTACK;
        validation.violations_found += invalid_count;
        validation.result = VAE_BBB_RESULT_BLOCKED;
        validation.severity = BBB_SEVERITY_CRITICAL;
        snprintf(validation.reason, sizeof(validation.reason),
                 "NaN/Inf in gradients - possible attack");
        bridge->stats.nan_inf_detected++;
    }

    /* Check gradient magnitude */
    if (validation.result != VAE_BBB_RESULT_BLOCKED) {
        float max_grad = get_max_magnitude(gradients);
        if (max_grad > bridge->config.bounds.gradient_max) {
            validation.threat = VAE_THREAT_GRADIENT_ATTACK;
            validation.violations_found++;
            validation.severity = BBB_SEVERITY_HIGH;

            if (bridge->config.strict_mode) {
                validation.result = VAE_BBB_RESULT_BLOCKED;
                snprintf(validation.reason, sizeof(validation.reason),
                         "Gradient magnitude %.2f exceeds max %.2f",
                         max_grad, bridge->config.bounds.gradient_max);
            } else {
                validation.result = VAE_BBB_RESULT_SANITIZED;
                validation.sanitized = true;
            }
        }
    }

    switch (validation.result) {
        case VAE_BBB_RESULT_PASS:
            bridge->stats.passed++;
            break;
        case VAE_BBB_RESULT_SANITIZED:
            bridge->stats.sanitized++;
            break;
        case VAE_BBB_RESULT_BLOCKED:
            bridge->stats.blocked++;
            break;
        default:
            break;
    }

    bridge->last_validation = validation;
    update_health(bridge, validation.result);

    nimcp_mutex_unlock(bridge->mutex);

    if (result) {
        *result = validation;
    }

    return validation.result;
}

vae_bbb_result_t vae_bbb_validate_pipeline(vae_bbb_bridge_t* bridge,
                                            const nimcp_tensor_t* input,
                                            const nimcp_tensor_t* latent,
                                            const nimcp_tensor_t* output,
                                            vae_bbb_validation_t* result)
{
    vae_bbb_validation_t stage_result;
    vae_bbb_result_t overall = VAE_BBB_RESULT_PASS;

    /* Validate input */
    if (input) {
        vae_bbb_result_t r = vae_bbb_validate_input(bridge, input, &stage_result);
        if (r == VAE_BBB_RESULT_BLOCKED) {
            if (result) *result = stage_result;
            return r;
        }
        if (r == VAE_BBB_RESULT_SANITIZED) overall = r;
    }

    /* Validate latent */
    if (latent) {
        vae_bbb_result_t r = vae_bbb_validate_latent(bridge, latent, &stage_result);
        if (r == VAE_BBB_RESULT_BLOCKED) {
            if (result) *result = stage_result;
            return r;
        }
        if (r == VAE_BBB_RESULT_SANITIZED) overall = r;
    }

    /* Validate output */
    if (output) {
        vae_bbb_result_t r = vae_bbb_validate_output(bridge, output, &stage_result);
        if (r == VAE_BBB_RESULT_BLOCKED) {
            if (result) *result = stage_result;
            return r;
        }
        if (r == VAE_BBB_RESULT_SANITIZED) overall = r;
    }

    if (result) {
        result->result = overall;
        result->stage = VAE_BBB_STAGE_OUTPUT;
    }

    return overall;
}

/* ============================================================================
 * Sanitization API
 * ============================================================================ */

int vae_bbb_sanitize_input(vae_bbb_bridge_t* bridge, nimcp_tensor_t* input)
{
    if (!bridge || !input || !input->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_sanitize_input: required parameter is NULL (bridge, input, input->data)");
        return -1;
    }

    uint32_t replaced = 0;
    uint32_t clamped = 0;

    /* Replace NaN/Inf */
    if (bridge->config.replace_nan) {
        replaced = vae_bbb_replace_invalid(bridge, input,
                                           bridge->config.nan_replacement);
    }

    /* Clamp values */
    if (bridge->config.clamp_values) {
        clamped = vae_bbb_clamp_tensor(bridge, input,
                                       bridge->config.bounds.input_min,
                                       bridge->config.bounds.input_max);
    }

    if (bridge->config.log_validations && (replaced > 0 || clamped > 0)) {
        NIMCP_LOG_DEBUG("VAE-BBB: Sanitized input (replaced=%u, clamped=%u)",
                        replaced, clamped);
    }

    return 0;
}

int vae_bbb_sanitize_output(vae_bbb_bridge_t* bridge, nimcp_tensor_t* output)
{
    if (!bridge || !output || !output->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_sanitize_output: required parameter is NULL (bridge, output, output->data)");
        return -1;
    }

    uint32_t replaced = 0;
    uint32_t clamped = 0;

    if (bridge->config.replace_nan) {
        replaced = vae_bbb_replace_invalid(bridge, output,
                                           bridge->config.nan_replacement);
    }

    if (bridge->config.clamp_values) {
        clamped = vae_bbb_clamp_tensor(bridge, output,
                                       bridge->config.bounds.output_min,
                                       bridge->config.bounds.output_max);
    }

    return 0;
}

uint32_t vae_bbb_clamp_tensor(vae_bbb_bridge_t* bridge, nimcp_tensor_t* tensor,
                               float min_val, float max_val)
{
    (void)bridge;
    if (!tensor || !tensor->data) return 0;

    uint32_t clamped = 0;
    uint32_t total = nimcp_tensor_numel(tensor);
    float* data = (float*)tensor->data;

    for (uint32_t i = 0; i < total; i++) {
        if (data[i] < min_val) {
            data[i] = min_val;
            clamped++;
        } else if (data[i] > max_val) {
            data[i] = max_val;
            clamped++;
        }
    }

    return clamped;
}

uint32_t vae_bbb_replace_invalid(vae_bbb_bridge_t* bridge, nimcp_tensor_t* tensor,
                                  float replacement)
{
    (void)bridge;
    if (!tensor || !tensor->data) return 0;

    uint32_t replaced = 0;
    uint32_t total = nimcp_tensor_numel(tensor);
    float* data = (float*)tensor->data;

    for (uint32_t i = 0; i < total; i++) {
        if (!is_valid_float(data[i])) {
            data[i] = replacement;
            replaced++;
        }
    }

    return replaced;
}

/* ============================================================================
 * Adversarial Detection API
 * ============================================================================ */

bool vae_bbb_detect_adversarial(vae_bbb_bridge_t* bridge,
                                 const nimcp_tensor_t* input,
                                 const nimcp_tensor_t* reference,
                                 float* adversarial_score)
{
    if (!bridge || !input || !adversarial_score) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_detect_adversarial: required parameter is NULL (bridge, input, adversarial_score)");
        return false;
    }
    if (!bridge->config.detect_adversarial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_detect_adversarial: bridge->config is NULL");
        return false;
    }

    *adversarial_score = 0.0f;

    if (!reference) {
        /* Without reference, use heuristics */
        float max_mag = get_max_magnitude(input);
        float thresh = bridge->config.bounds.input_max * 0.9f;
        if (max_mag > thresh) {
            *adversarial_score = (max_mag - thresh) / thresh;
        }
    } else {
        /* Compute perturbation */
        float perturbation = vae_bbb_perturbation_magnitude(input, reference);
        *adversarial_score = perturbation;
    }

    bool is_adversarial = *adversarial_score > bridge->config.adversarial_threshold;

    if (is_adversarial) {
        nimcp_mutex_lock(bridge->mutex);
        bridge->stats.adversarial_detected++;
        nimcp_mutex_unlock(bridge->mutex);

        if (bridge->config.log_validations) {
            NIMCP_LOG_WARN("VAE-BBB: Adversarial input detected (score=%.4f)",
                           *adversarial_score);
        }
    }

    return is_adversarial;
}

float vae_bbb_perturbation_magnitude(const nimcp_tensor_t* input,
                                      const nimcp_tensor_t* reference)
{
    if (!input || !reference || !input->data || !reference->data) return 0.0f;

    uint32_t total = nimcp_tensor_numel(input);
    uint32_t ref_total = nimcp_tensor_numel(reference);
    if (total != ref_total) return -1.0f;

    const float* in_data = (const float*)input->data;
    const float* ref_data = (const float*)reference->data;

    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < total; i++) {
        float diff = in_data[i] - ref_data[i];
        sum_sq += diff * diff;
    }

    return sqrtf(sum_sq / (total > 0 ? total : 1));
}

/* ============================================================================
 * BBB Integration API
 * ============================================================================ */

int vae_bbb_report_threat(vae_bbb_bridge_t* bridge,
                           vae_bbb_threat_t threat,
                           bbb_severity_t severity,
                           const void* data,
                           size_t data_size)
{
    if (!bridge || !bridge->bbb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_report_threat: required parameter is NULL (bridge, bridge->bbb)");
        return -1;
    }

    /* Convert VAE threat to BBB threat */
    bbb_threat_type_t bbb_threat = vae_threat_to_bbb_threat(threat);

    /* Use BBB validation to report */
    bbb_validation_result_t result;
    bbb_validate_input(bridge->bbb, data, data_size, &result);

    nimcp_mutex_lock(bridge->mutex);
    bridge->stats.reports_to_bbb++;
    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_INFO("VAE-BBB: Threat reported to BBB (type=%s, severity=%d)",
                   vae_bbb_threat_to_string(threat), severity);

    return 0;
}

uint32_t vae_bbb_quarantine_tensor(vae_bbb_bridge_t* bridge,
                                    const nimcp_tensor_t* tensor,
                                    const char* reason)
{
    if (!bridge || !tensor || !bridge->config.quarantine_threats) return 0;

    /* In full implementation, would store tensor for analysis */
    nimcp_mutex_lock(bridge->mutex);
    bridge->stats.quarantined++;
    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOG_WARN("VAE-BBB: Tensor quarantined: %s", reason ? reason : "unknown");

    return 1;  /* Return quarantine ID */
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int vae_bbb_bridge_get_stats(const vae_bbb_bridge_t* bridge,
                              vae_bbb_bridge_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    stats->uptime_us = get_timestamp_us() - bridge->creation_time_us;
    return 0;
}

int vae_bbb_bridge_get_health(const vae_bbb_bridge_t* bridge,
                               vae_bbb_bridge_health_t* health)
{
    if (!bridge || !health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_bridge_get_health: required parameter is NULL (bridge, health)");
        return -1;
    }
    *health = bridge->health;
    return 0;
}

vae_bbb_bridge_state_t vae_bbb_bridge_get_state(const vae_bbb_bridge_t* bridge)
{
    if (!bridge) return VAE_BBB_STATE_DISCONNECTED;
    return bridge->state;
}

int vae_bbb_bridge_get_last_validation(const vae_bbb_bridge_t* bridge,
                                        vae_bbb_validation_t* result)
{
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_bridge_get_last_validation: required parameter is NULL (bridge, result)");
        return -1;
    }
    *result = bridge->last_validation;
    return 0;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int vae_bbb_set_bounds(vae_bbb_bridge_t* bridge, const vae_bbb_bounds_t* bounds)
{
    if (!bridge || !bounds) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_set_bounds: required parameter is NULL (bridge, bounds)");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);
    bridge->config.bounds = *bounds;
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int vae_bbb_get_bounds(const vae_bbb_bridge_t* bridge, vae_bbb_bounds_t* bounds)
{
    if (!bridge || !bounds) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_get_bounds: required parameter is NULL (bridge, bounds)");
        return -1;
    }
    *bounds = bridge->config.bounds;
    return 0;
}

int vae_bbb_set_validation_mask(vae_bbb_bridge_t* bridge, uint32_t mask)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_bbb_set_validation_mask: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->mutex);
    bridge->config.validation_mask = mask;
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

bbb_threat_type_t vae_threat_to_bbb_threat(vae_bbb_threat_t threat)
{
    switch (threat) {
        case VAE_THREAT_ADVERSARIAL_INPUT:
            return BBB_THREAT_CODE_INJECTION;
        case VAE_THREAT_NAN_INF:
            return BBB_THREAT_DATA_TAMPERING;
        case VAE_THREAT_OUT_OF_BOUNDS:
            return BBB_THREAT_BUFFER_OVERFLOW;
        case VAE_THREAT_MALFORMED_TENSOR:
            return BBB_THREAT_DATA_TAMPERING;
        case VAE_THREAT_GRADIENT_ATTACK:
            return BBB_THREAT_CODE_INJECTION;
        case VAE_THREAT_LATENT_INJECTION:
            return BBB_THREAT_CODE_INJECTION;
        case VAE_THREAT_RECONSTRUCTION_ATTACK:
            return BBB_THREAT_DATA_TAMPERING;
        default:
            return BBB_THREAT_UNKNOWN;
    }
}

const char* vae_bbb_threat_to_string(vae_bbb_threat_t threat)
{
    switch (threat) {
        case VAE_THREAT_NONE:               return "NONE";
        case VAE_THREAT_ADVERSARIAL_INPUT:  return "ADVERSARIAL_INPUT";
        case VAE_THREAT_NAN_INF:            return "NAN_INF";
        case VAE_THREAT_OUT_OF_BOUNDS:      return "OUT_OF_BOUNDS";
        case VAE_THREAT_MALFORMED_TENSOR:   return "MALFORMED_TENSOR";
        case VAE_THREAT_GRADIENT_ATTACK:    return "GRADIENT_ATTACK";
        case VAE_THREAT_LATENT_INJECTION:   return "LATENT_INJECTION";
        case VAE_THREAT_RECONSTRUCTION_ATTACK: return "RECONSTRUCTION_ATTACK";
        default:                            return "UNKNOWN";
    }
}

const char* vae_bbb_result_to_string(vae_bbb_result_t result)
{
    switch (result) {
        case VAE_BBB_RESULT_PASS:       return "PASS";
        case VAE_BBB_RESULT_SANITIZED:  return "SANITIZED";
        case VAE_BBB_RESULT_BLOCKED:    return "BLOCKED";
        case VAE_BBB_RESULT_QUARANTINED: return "QUARANTINED";
        case VAE_BBB_RESULT_ERROR:      return "ERROR";
        default:                        return "UNKNOWN";
    }
}
