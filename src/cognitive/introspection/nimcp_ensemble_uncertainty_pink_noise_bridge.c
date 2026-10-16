//=============================================================================
// nimcp_ensemble_uncertainty_pink_noise_bridge.c
//=============================================================================
/**
 * @file nimcp_ensemble_uncertainty_pink_noise_bridge.c
 * @brief Implementation of Ensemble Uncertainty Pink Noise Bridge
 *
 * WHAT: Bidirectional integration between pink noise and ensemble uncertainty
 * WHY:  Add biologically realistic 1/f noise to ensemble predictions
 * HOW:  Generate pink noise per model, inject into predictions, adapt based on uncertainty
 */

#include "cognitive/introspection/nimcp_ensemble_uncertainty_pink_noise_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "[ENSEMBLE_PINK]"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(ensemble_uncertainty_pink_noise_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_ensemble_uncertainty_pink_noise_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_ensemble_uncertainty_pink_noise_bridge_mesh_registry = NULL;

nimcp_error_t ensemble_uncertainty_pink_noise_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_ensemble_uncertainty_pink_noise_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "ensemble_uncertainty_pink_noise_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "ensemble_uncertainty_pink_noise_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_ensemble_uncertainty_pink_noise_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_ensemble_uncertainty_pink_noise_bridge_mesh_registry = registry;
    return err;
}

void ensemble_uncertainty_pink_noise_bridge_mesh_unregister(void) {
    if (g_ensemble_uncertainty_pink_noise_bridge_mesh_registry && g_ensemble_uncertainty_pink_noise_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_ensemble_uncertainty_pink_noise_bridge_mesh_registry, g_ensemble_uncertainty_pink_noise_bridge_mesh_id);
        g_ensemble_uncertainty_pink_noise_bridge_mesh_id = 0;
        g_ensemble_uncertainty_pink_noise_bridge_mesh_registry = NULL;
    }
}


static inline void ensemble_uncertainty_pink_noise_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_ensemble_uncertainty_pink_noise_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_ensemble_uncertainty_pink_noise_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_ensemble_uncertainty_pink_noise_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Exponential moving average update
 * WHY:  Smooth statistics over time
 * HOW:  avg_new = alpha * value + (1-alpha) * avg_old
 */
static inline float ema_update(float avg, float value, float alpha) {
    return alpha * value + (1.0f - alpha) * avg;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

int ensemble_pink_default_config(ensemble_pink_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_defaul", 0.0f);


    NIMCP_CHECK_THROW(config != NULL, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(ensemble_pink_config_t));

    // Noise generation parameters
    config->base_amplitude = ENSEMBLE_PINK_DEFAULT_AMPLITUDE;
    config->base_alpha = ENSEMBLE_PINK_DEFAULT_ALPHA;
    config->sample_rate = 1000.0f;
    config->noise_method = PINK_NOISE_VOSS;

    // Injection parameters
    config->injection_mode = ENSEMBLE_PINK_INJECT_PREDICTIONS;
    config->per_model_noise = true;
    config->feature_noise_scale = 0.1f;
    config->weight_noise_scale = 0.05f;
    config->prediction_noise_scale = 1.0f;

    // Adaptation parameters
    config->adaptation_mode = ENSEMBLE_PINK_ADAPT_EPISTEMIC;
    config->adaptation_rate = ENSEMBLE_PINK_ADAPTATION_RATE;
    config->epistemic_target = 0.2f;
    config->aleatoric_target = 0.1f;
    config->amplitude_min = ENSEMBLE_PINK_MIN_AMPLITUDE;
    config->amplitude_max = ENSEMBLE_PINK_MAX_AMPLITUDE;

    // Temporal parameters
    config->correlation_time_ms = 100.0f;
    config->enable_temporal_smoothing = true;
    config->smoothing_alpha = 0.1f;

    // Feature flags
    config->enable_noise_injection = true;
    config->enable_adaptation = true;
    config->enable_feedback = true;
    config->enable_logging = false;

    return NIMCP_SUCCESS;
}

ensemble_pink_bridge_t* ensemble_pink_create(const ensemble_pink_config_t* config) {
    // Allocate bridge structure
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_create", 0.0f);


    ensemble_pink_bridge_t* bridge = (ensemble_pink_bridge_t*)nimcp_malloc(
        sizeof(ensemble_pink_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(ensemble_pink_bridge_t));

    // Copy configuration or use defaults
    if (config) {
        bridge->config = *config;
    } else {
        int ret = ensemble_pink_default_config(&bridge->config);
        if (ret != NIMCP_SUCCESS) {
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensemble_pink_create: validation failed");
            return NULL;
        }
    }

    // Create mutex for thread safety
    if (bridge_base_init(&bridge->base, 0, "ensemble_uncertainty_pink_nois") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ensemble_pink_create: bridge->base is NULL");
        return NULL;
    }

    // Initialize state
    bridge->state.current_amplitude = bridge->config.base_amplitude;
    bridge->state.current_alpha = bridge->config.base_alpha;

    // Initialize effects
    bridge->effects.amplitude_modifier = 1.0f;
    bridge->effects.alpha_modifier = 0.0f;
    bridge->effects.effective_amplitude = bridge->config.base_amplitude;
    bridge->effects.effective_alpha = bridge->config.base_alpha;

    NIMCP_LOGGING_INFO(LOG_MODULE " Bridge created successfully");

    return bridge;
}

void ensemble_pink_destroy(ensemble_pink_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    // Disconnect bio-async if connected
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_destro", 0.0f);


    if (bridge->base.bio_async_enabled) {
        ensemble_pink_disconnect_bio_async(bridge);
    }

    // Disconnect from ensemble (destroys generators)
    if (bridge->ensemble) {
        ensemble_pink_disconnect_ensemble(bridge);
    }

    // Destroy mutex
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    // Free bridge
    nimcp_free(bridge);
    bridge = NULL;
}

//=============================================================================
// Connection Functions
//=============================================================================

int ensemble_pink_connect_ensemble(
    ensemble_pink_bridge_t* bridge,
    ensemble_context_t ensemble)
{
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_connec", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(ensemble != NULL, NIMCP_ERROR_NULL_POINTER, "ensemble is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    // Store ensemble reference
    bridge->ensemble = ensemble;

    // Get ensemble size (number of models)
    uint32_t num_models = ensemble_get_size(ensemble);
    if (num_models == 0) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Ensemble has no models");
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_STATE;
    }

    // Determine number of generators needed
    bridge->num_generators = bridge->config.per_model_noise ? num_models : 1;

    // Allocate generator array
    bridge->generators = (pink_noise_generator_t*)nimcp_malloc(
        bridge->num_generators * sizeof(pink_noise_generator_t)
    );
    if (!bridge->generators) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to allocate generators");
        bridge->ensemble = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    // Create pink noise configuration
    pink_noise_config_t noise_config = pink_noise_default_config();
    noise_config.alpha = bridge->config.base_alpha;
    noise_config.amplitude = bridge->config.base_amplitude;
    noise_config.sample_rate = bridge->config.sample_rate;
    noise_config.method = bridge->config.noise_method;

    // Create generators
    for (uint32_t i = 0; i < bridge->num_generators; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_generators > 256) {
            ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)bridge->num_generators);
        }

        noise_config.seed = i + 1;  // Different seed per generator
        bridge->generators[i] = pink_noise_create(&noise_config);
        if (!bridge->generators[i]) {
            NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to create generator %u", i);
            // Cleanup already created generators
            for (uint32_t j = 0; j < i; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && i > 256) {
                    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_loop",
                                     (float)(j + 1) / (float)i);
                }

                pink_noise_destroy(bridge->generators[j]);
            }
            nimcp_free(bridge->generators);
            bridge->generators = NULL;
            bridge->ensemble = NULL;
            nimcp_mutex_unlock(bridge->base.mutex);
            return NIMCP_ERROR_OPERATION_FAILED;
        }
    }

    // Allocate noise sample buffer
    bridge->state.num_noise_samples = num_models;
    bridge->state.noise_samples = (float*)nimcp_malloc(
        num_models * sizeof(float)
    );
    if (!bridge->state.noise_samples) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to allocate noise buffer");
        for (uint32_t i = 0; i < bridge->num_generators; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_generators > 256) {
                ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_loop",
                                 (float)(i + 1) / (float)bridge->num_generators);
            }

            pink_noise_destroy(bridge->generators[i]);
        }
        nimcp_free(bridge->generators);
        bridge->generators = NULL;
        bridge->ensemble = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    memset(bridge->state.noise_samples, 0, num_models * sizeof(float));

    NIMCP_LOGGING_INFO(LOG_MODULE " Connected to ensemble with %u models, %u generators",
                       num_models, bridge->num_generators);

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int ensemble_pink_disconnect_ensemble(ensemble_pink_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_discon", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    // Destroy generators
    if (bridge->generators) {
        for (uint32_t i = 0; i < bridge->num_generators; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_generators > 256) {
                ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_loop",
                                 (float)(i + 1) / (float)bridge->num_generators);
            }

            pink_noise_destroy(bridge->generators[i]);
        }
        nimcp_free(bridge->generators);
        bridge->generators = NULL;
        bridge->num_generators = 0;
    }

    // Free noise samples
    if (bridge->state.noise_samples) {
        nimcp_free(bridge->state.noise_samples);
        bridge->state.noise_samples = NULL;
        bridge->state.num_noise_samples = 0;
    }

    // Clear ensemble reference
    bridge->ensemble = NULL;

    NIMCP_LOGGING_INFO(LOG_MODULE " Disconnected from ensemble");

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

bool ensemble_pink_is_connected(const ensemble_pink_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_is_con", 0.0f);


    return bridge->ensemble != NULL;
}

//=============================================================================
// Noise Injection Functions
//=============================================================================

int ensemble_pink_generate_noise(ensemble_pink_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_genera", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->ensemble != NULL && bridge->generators != NULL,
                      NIMCP_ERROR_INVALID_STATE, "ensemble or generators not connected");

    nimcp_mutex_lock(bridge->base.mutex);

    // Generate noise samples
    uint32_t num_models = bridge->state.num_noise_samples;

    if (bridge->config.per_model_noise) {
        // Independent noise per model
        for (uint32_t i = 0; i < num_models; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_models > 256) {
                ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_loop",
                                 (float)(i + 1) / (float)num_models);
            }

            float sample = 0.0f;
            if (!pink_noise_generate_sample(bridge->generators[i], &sample)) {
                NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to generate noise sample %u", i);
                nimcp_mutex_unlock(bridge->base.mutex);
                return NIMCP_ERROR_OPERATION_FAILED;
            }
            // Apply current amplitude modulation
            bridge->state.noise_samples[i] = sample * bridge->effects.effective_amplitude;
        }
    } else {
        // Shared noise for all models
        float sample = 0.0f;
        if (!pink_noise_generate_sample(bridge->generators[0], &sample)) {
            NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to generate shared noise sample");
            nimcp_mutex_unlock(bridge->base.mutex);
            return NIMCP_ERROR_OPERATION_FAILED;
        }
        float modulated_sample = sample * bridge->effects.effective_amplitude;
        for (uint32_t i = 0; i < num_models; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_models > 256) {
                ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_loop",
                                 (float)(i + 1) / (float)num_models);
            }

            bridge->state.noise_samples[i] = modulated_sample;
        }
    }

    // Apply temporal smoothing if enabled
    if (bridge->config.enable_temporal_smoothing) {
        float alpha = bridge->config.smoothing_alpha;
        for (uint32_t i = 0; i < num_models; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_models > 256) {
                ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_loop",
                                 (float)(i + 1) / (float)num_models);
            }

            bridge->state.noise_samples[i] = ema_update(
                bridge->state.temporal_smoothing_state,
                bridge->state.noise_samples[i],
                alpha
            );
        }
        // Update smoothing state (use mean of samples)
        float mean_sample = 0.0f;
        for (uint32_t i = 0; i < num_models; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_models > 256) {
                ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_loop",
                                 (float)(i + 1) / (float)num_models);
            }

            mean_sample += bridge->state.noise_samples[i];
        }
        bridge->state.temporal_smoothing_state = mean_sample / (float)num_models;
    }

    bridge->state.samples_generated++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int ensemble_pink_inject_noise(
    ensemble_pink_bridge_t* bridge,
    ensemble_prediction_t* predictions,
    uint32_t num_predictions)
{
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_inject", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(predictions != NULL, NIMCP_ERROR_NULL_POINTER, "predictions is NULL");

    if (!bridge->config.enable_noise_injection) {
        return NIMCP_SUCCESS;  // Noise disabled, no-op
    }

    if (!bridge->ensemble || !bridge->generators) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    // Generate fresh noise samples
    int ret = ensemble_pink_generate_noise(bridge);
    if (ret != NIMCP_SUCCESS) {
        return ret;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    // Inject noise based on mode
    float scale = bridge->config.prediction_noise_scale;

    for (uint32_t i = 0; i < num_predictions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_predictions > 256) {
            ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)num_predictions);
        }

        float noise = bridge->state.noise_samples[i];

        // Apply to prediction vector
        for (uint32_t j = 0; j < predictions[i].size; j++) {
            switch (bridge->config.injection_mode) {
                case ENSEMBLE_PINK_INJECT_PREDICTIONS:
                    // Additive noise
                    predictions[i].prediction[j] += noise * scale;
                    break;

                case ENSEMBLE_PINK_INJECT_COMBINED:
                    // Multiplicative + additive
                    predictions[i].prediction[j] *= (1.0f + noise * scale * 0.5f);
                    predictions[i].prediction[j] += noise * scale * 0.5f;
                    break;

                default:
                    // Other modes handled separately
                    break;
            }
        }
    }

    bridge->stats.total_injections++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG(LOG_MODULE " Injected noise into %u predictions", num_predictions);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int ensemble_pink_inject_feature_noise(
    ensemble_pink_bridge_t* bridge,
    float* features,
    uint32_t num_features)
{
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_inject", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(features != NULL, NIMCP_ERROR_NULL_POINTER, "features is NULL");

    if (!bridge->config.enable_noise_injection) {
        return NIMCP_SUCCESS;
    }

    if (!bridge->generators) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float scale = bridge->config.feature_noise_scale;

    // Generate and apply noise to each feature
    for (uint32_t i = 0; i < num_features; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_features > 256) {
            ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_loop",
                             (float)(i + 1) / (float)num_features);
        }

        float sample = 0.0f;
        pink_noise_generate_sample(bridge->generators[0], &sample);
        features[i] += sample * scale * bridge->effects.effective_amplitude;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Adaptation Functions
//=============================================================================

int ensemble_pink_update_uncertainty(
    ensemble_pink_bridge_t* bridge,
    const ensemble_uncertainty_result_t* uncertainty)
{
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_update", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(uncertainty != NULL, NIMCP_ERROR_NULL_POINTER, "uncertainty is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    // Update uncertainty metrics
    bridge->uncertainty.epistemic = uncertainty->epistemic;
    bridge->uncertainty.aleatoric = uncertainty->aleatoric;
    bridge->uncertainty.total = uncertainty->total;
    bridge->uncertainty.confidence = uncertainty->confidence;

    // Compute ratio (avoid division by zero)
    float total = uncertainty->epistemic + uncertainty->aleatoric;
    if (total > 1e-6f) {
        bridge->uncertainty.epistemic_aleatoric_ratio =
            uncertainty->epistemic / (fabsf(total) > 1e-7f ? total : 1e-7f);
    } else {
        bridge->uncertainty.epistemic_aleatoric_ratio = 0.5f;
    }

    // Update running statistics
    float alpha = 0.01f;  // Slow EMA for statistics
    bridge->stats.avg_epistemic = ema_update(
        bridge->stats.avg_epistemic,
        uncertainty->epistemic,
        alpha
    );
    bridge->stats.avg_aleatoric = ema_update(
        bridge->stats.avg_aleatoric,
        uncertainty->aleatoric,
        alpha
    );

    // Track min/max
    if (bridge->stats.update_count == 0) {
        bridge->stats.min_uncertainty = total;
        bridge->stats.max_uncertainty = total;
    } else {
        if (total < bridge->stats.min_uncertainty) {
            bridge->stats.min_uncertainty = total;
        }
        if (total > bridge->stats.max_uncertainty) {
            bridge->stats.max_uncertainty = total;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int ensemble_pink_adapt_noise(ensemble_pink_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_adapt_", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->config.enable_adaptation) {
        return NIMCP_SUCCESS;  // Adaptation disabled
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float amplitude_modifier = 1.0f;
    float alpha_modifier = 0.0f;
    bool adapted = false;

    // Apply adaptation strategy
    switch (bridge->config.adaptation_mode) {
        case ENSEMBLE_PINK_ADAPT_EPISTEMIC: {
            // Adapt amplitude based on epistemic uncertainty
            float error = bridge->uncertainty.epistemic - bridge->config.epistemic_target;
            float adjustment = bridge->config.adaptation_rate * error;
            amplitude_modifier = 1.0f + adjustment;
            adapted = (fabsf(error) > 0.01f);
            break;
        }

        case ENSEMBLE_PINK_ADAPT_ALEATORIC: {
            // Adapt amplitude based on aleatoric uncertainty
            float error = bridge->uncertainty.aleatoric - bridge->config.aleatoric_target;
            float adjustment = bridge->config.adaptation_rate * error;
            amplitude_modifier = 1.0f + adjustment;
            adapted = (fabsf(error) > 0.01f);
            break;
        }

        case ENSEMBLE_PINK_ADAPT_COMBINED: {
            // Adapt based on total uncertainty
            float target_total = bridge->config.epistemic_target +
                                bridge->config.aleatoric_target;
            float error = bridge->uncertainty.total - target_total;
            float adjustment = bridge->config.adaptation_rate * error;
            amplitude_modifier = 1.0f + adjustment;
            adapted = (fabsf(error) > 0.01f);
            break;
        }

        case ENSEMBLE_PINK_ADAPT_RATIO: {
            // Adapt based on epistemic/aleatoric ratio
            float ratio = bridge->uncertainty.epistemic_aleatoric_ratio;
            // If ratio > 0.5, model is uncertain → increase noise
            // If ratio < 0.5, data is noisy → decrease noise
            float adjustment = bridge->config.adaptation_rate * (ratio - 0.5f);
            amplitude_modifier = 1.0f + adjustment * 2.0f;
            adapted = (fabsf(ratio - 0.5f) > 0.1f);
            break;
        }

        case ENSEMBLE_PINK_ADAPT_NONE:
        default:
            // No adaptation
            break;
    }

    // Apply modifiers
    bridge->effects.amplitude_modifier = amplitude_modifier;
    bridge->effects.alpha_modifier = alpha_modifier;

    // Compute effective parameters
    float new_amplitude = bridge->config.base_amplitude * amplitude_modifier;
    float new_alpha = bridge->config.base_alpha + alpha_modifier;

    // Clamp to valid ranges
    new_amplitude = nimcp_clampf(new_amplitude,
                            bridge->config.amplitude_min,
                            bridge->config.amplitude_max);
    new_alpha = nimcp_clampf(new_alpha, 0.0f, 2.0f);

    bridge->effects.effective_amplitude = new_amplitude;
    bridge->effects.effective_alpha = new_alpha;
    bridge->effects.adaptation_triggered = adapted;

    // Update state
    bridge->state.current_amplitude = new_amplitude;
    bridge->state.current_alpha = new_alpha;

    if (adapted) {
        bridge->stats.total_adaptations++;
    }

    // Update statistics
    float alpha = 0.01f;
    bridge->stats.avg_amplitude = ema_update(
        bridge->stats.avg_amplitude,
        new_amplitude,
        alpha
    );
    bridge->stats.avg_alpha = ema_update(
        bridge->stats.avg_alpha,
        new_alpha,
        alpha
    );

    if (bridge->config.enable_logging && adapted) {
        NIMCP_LOGGING_DEBUG(LOG_MODULE " Adapted: amp=%.4f (mod=%.2f), alpha=%.2f",
                           new_amplitude, amplitude_modifier, new_alpha);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int ensemble_pink_update(
    ensemble_pink_bridge_t* bridge,
    uint64_t delta_ms)
{
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_update", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    // Adapt noise parameters based on uncertainty
    int ret = ensemble_pink_adapt_noise(bridge);
    if (ret != NIMCP_SUCCESS) {
        return ret;
    }

    // Update statistics
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.update_count++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Query Functions
//=============================================================================

float ensemble_pink_get_amplitude(const ensemble_pink_bridge_t* bridge) {
    if (!bridge) {
        return 0.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_get_am", 0.0f);


    return bridge->effects.effective_amplitude;
}

float ensemble_pink_get_alpha(const ensemble_pink_bridge_t* bridge) {
    if (!bridge) {
        return 0.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_get_al", 0.0f);


    return bridge->effects.effective_alpha;
}

const float* ensemble_pink_get_noise_samples(
    const ensemble_pink_bridge_t* bridge,
    uint32_t* num_samples)
{
    if (!bridge || !num_samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensemble_pink_get_noise_samples: required parameter is NULL (bridge, num_samples)");
        return NULL;
    }

    *num_samples = bridge->state.num_noise_samples;
    return bridge->state.noise_samples;
}

int ensemble_pink_get_stats(
    const ensemble_pink_bridge_t* bridge,
    ensemble_pink_stats_t* stats)
{
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_get_st", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL && stats != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in get_stats");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int ensemble_pink_get_uncertainty(
    const ensemble_pink_bridge_t* bridge,
    ensemble_pink_uncertainty_t* uncertainty)
{
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_get_un", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL && uncertainty != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in get_uncertainty");

    nimcp_mutex_lock(bridge->base.mutex);
    *uncertainty = bridge->uncertainty;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Control Functions
//=============================================================================

int ensemble_pink_set_enabled(
    ensemble_pink_bridge_t* bridge,
    bool enable)
{
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_set_en", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.enable_noise_injection = enable;
    NIMCP_LOGGING_INFO(LOG_MODULE " Noise injection %s",
                       enable ? "enabled" : "disabled");
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int ensemble_pink_set_adaptation_enabled(
    ensemble_pink_bridge_t* bridge,
    bool enable)
{
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_set_ad", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.enable_adaptation = enable;
    NIMCP_LOGGING_INFO(LOG_MODULE " Adaptation %s",
                       enable ? "enabled" : "disabled");
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int ensemble_pink_reset(ensemble_pink_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_reset", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    // Reset generators
    if (bridge->generators) {
        for (uint32_t i = 0; i < bridge->num_generators; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_generators > 256) {
                ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_loop",
                                 (float)(i + 1) / (float)bridge->num_generators);
            }

            pink_noise_reset(bridge->generators[i], i + 1);
        }
    }

    // Reset state
    bridge->state.current_amplitude = bridge->config.base_amplitude;
    bridge->state.current_alpha = bridge->config.base_alpha;
    bridge->state.samples_generated = 0;
    bridge->state.temporal_smoothing_state = 0.0f;

    if (bridge->state.noise_samples) {
        memset(bridge->state.noise_samples, 0,
               bridge->state.num_noise_samples * sizeof(float));
    }

    // Reset effects
    bridge->effects.amplitude_modifier = 1.0f;
    bridge->effects.alpha_modifier = 0.0f;
    bridge->effects.effective_amplitude = bridge->config.base_amplitude;
    bridge->effects.effective_alpha = bridge->config.base_alpha;
    bridge->effects.adaptation_triggered = false;

    // Reset statistics
    memset(&bridge->stats, 0, sizeof(ensemble_pink_stats_t));

    // Reset uncertainty
    memset(&bridge->uncertainty, 0, sizeof(ensemble_pink_uncertainty_t));

    NIMCP_LOGGING_INFO(LOG_MODULE " Bridge reset to initial state");

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int ensemble_pink_connect_bio_async(ensemble_pink_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_connec", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  // Already connected
    }

    nimcp_mutex_lock(bridge->base.mutex);

    // Register with bio-async router
    bio_module_info_t info = {
        .module_id = BIO_MODULE_INTROSPECTION,  // Use introspection module ID
        .module_name = "ensemble_pink_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO(LOG_MODULE " Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN(LOG_MODULE " Bio-async router not available");
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int ensemble_pink_disconnect_bio_async(ensemble_pink_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_discon", 0.0f);


    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  // Not connected
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->base.bio_ctx) {
        // Unregister from router (if API exists)
        // bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO(LOG_MODULE " Disconnected from bio-async router");

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

bool ensemble_pink_is_bio_async_connected(
    const ensemble_pink_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_is_bio", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ========================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Query knowledge graph for self-knowledge about ensemble pink noise bridge
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int ensemble_pink_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_ensemble_pink_bridge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Ensemble_Pink_Noise_Bridge");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                ensemble_uncertainty_pink_noise_bridge_heartbeat("ensemble_unc_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG(LOG_MODULE " self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Ensemble_Pink_Noise_Bridge");
    if (connections) {
        NIMCP_LOGGING_DEBUG(LOG_MODULE " has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Ensemble_Pink_Noise_Bridge");
    if (incoming) {
        NIMCP_LOGGING_DEBUG(LOG_MODULE " has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Full Training
 * ============================================================================ */

void ensemble_uncertainty_pink_noise_bridge_set_instance_health_agent(
    ensemble_pink_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

int ensemble_uncertainty_pink_noise_bridge_training_begin(ensemble_pink_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ensemble_uncertainty_pink_noise_bridge_training_begin: NULL argument");
        return -1;
    }
    ensemble_uncertainty_pink_noise_bridge_heartbeat_instance(bridge->health_agent,
        "ens_pink_training_begin", 0.0f);
    bridge->stats.total_injections = 0;
    bridge->stats.avg_amplitude = 0.0f;
    bridge->state.current_amplitude = 0.5f;
    NIMCP_LOGGING_INFO("[ENS_PINK] Training begin: counters reset, baseline state initialized");
    return 0;
}

int ensemble_uncertainty_pink_noise_bridge_training_step(ensemble_pink_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ensemble_uncertainty_pink_noise_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ensemble_uncertainty_pink_noise_bridge_heartbeat_instance(bridge->health_agent,
        "ens_pink_training_step", progress);
    float lr = bridge->config.base_amplitude;
    float adaptation = lr * (1.0f - progress) * 0.1f;
    bridge->config.base_amplitude = lr + adaptation;
    if (bridge->config.base_amplitude > 1.0f) bridge->config.base_amplitude = 1.0f;
    if (bridge->config.base_amplitude < 0.001f) bridge->config.base_amplitude = 0.001f;
    bridge->state.current_amplitude = bridge->state.current_amplitude * 0.99f + progress * 0.01f;
    bridge->stats.total_injections++;
    return 0;
}

int ensemble_uncertainty_pink_noise_bridge_training_end(ensemble_pink_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ensemble_uncertainty_pink_noise_bridge_training_end: NULL argument");
        return -1;
    }
    ensemble_uncertainty_pink_noise_bridge_heartbeat_instance(bridge->health_agent,
        "ens_pink_training_end", 1.0f);
    if (bridge->state.current_amplitude < 0.0f) bridge->state.current_amplitude = 0.0f;
    if (bridge->state.current_amplitude > 1.0f) bridge->state.current_amplitude = 1.0f;
    NIMCP_LOGGING_INFO("[ENS_PINK] Training end: amplitude=%.3f, injections=%u",
        bridge->state.current_amplitude, bridge->stats.total_injections);
    return 0;
}
