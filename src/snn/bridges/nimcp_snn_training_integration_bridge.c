/**
 * @file nimcp_snn_training_integration_bridge.c
 * @brief SNN-Training Integration Bridge Implementation
 *
 * WHAT: Implements bridge connecting SNN training to NIMCP training pipeline
 * WHY:  Enable spike-based metrics and pipeline parameter modulation
 * HOW:  Aggregates SNN metrics, translates pipeline parameters to SNN params
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_training_integration_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_training.h"
#include "snn/nimcp_snn_network.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_training_integration_bridge)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Connected SNN context entry
 */
typedef struct {
    snn_training_ctx_t* ctx;
    snn_network_t* network;
    char name[NIMCP_ID_BUFFER_SIZE];
    bool active;
    uint64_t ltp_count;
    uint64_t ltd_count;
    float total_delta_w;
} snn_context_entry_t;

/**
 * @brief Internal bridge structure
 */
struct snn_training_integration_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    snn_training_integration_config_t config;

    /* Connected SNN contexts */
    snn_context_entry_t contexts[SNN_TRAINING_INTEGRATION_MAX_CONTEXTS];
    uint32_t num_contexts;

    /* Current metrics */
    snn_training_metrics_t metrics;

    /* Pipeline parameters */
    snn_pipeline_params_t params;

    /* Current state */
    snn_training_integration_state_t state;

    /* Statistics */
    snn_training_integration_stats_t stats;

    /* Connected bridges */
    nimcp_brain_training_ctx_t* brain_training;
    cognitive_training_bridge_t* cognitive_training;
    training_immune_system_t* training_immune;
    training_plasticity_bridge_t* training_plasticity;

    /* Reward accumulation */
    float reward_by_source[SNN_REWARD_SOURCE_COUNT];
    float reward_cumulative;

    /* Bio-async context */
    void* bio_ctx;
    /* Thread safety */
    nimcp_platform_mutex_t* mutex;

    /* Operational flags */
    bool started;
    bool learning_paused;
    bool consolidation_active;
    bool exploration_active;
    bool emergency_brake_active;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute learning stability from metrics
 */
static float compute_learning_stability(const snn_training_metrics_t* metrics) {
    if (!metrics) return 0.0f;

    /* Weight saturation penalty */
    float saturation_penalty = metrics->weight_saturation_ratio;

    /* Variance penalty (normalized) */
    float variance_penalty = fminf(1.0f, metrics->weight_variance / 10.0f);

    /* Rate deviation penalty */
    float rate_penalty = fminf(1.0f, fabsf(metrics->rate_deviation) / 50.0f);

    /* Compute stability as inverse of penalties */
    float stability = 1.0f - 0.4f * saturation_penalty
                          - 0.3f * variance_penalty
                          - 0.3f * rate_penalty;

    return fmaxf(0.0f, fminf(1.0f, stability));
}

/**
 * @brief Aggregate metrics from all connected contexts
 */
static void aggregate_context_metrics(snn_training_integration_bridge_t* bridge) {
    if (!bridge) return;

    /* Reset aggregated metrics */
    memset(&bridge->metrics, 0, sizeof(snn_training_metrics_t));

    uint32_t active_count = 0;
    float total_firing_rate = 0.0f;
    float total_weight_mean = 0.0f;

    for (uint32_t i = 0; i < bridge->num_contexts; i++) {
        if (!bridge->contexts[i].active) continue;
        if (!bridge->contexts[i].ctx) continue;

        active_count++;

        /* Aggregate event counts */
        bridge->metrics.ltp_count += bridge->contexts[i].ltp_count;
        bridge->metrics.ltd_count += bridge->contexts[i].ltd_count;
        bridge->metrics.total_delta_w += bridge->contexts[i].total_delta_w;

        /* Get stats from context */
        uint64_t weight_updates, training_steps;
        float total_delta;
        snn_training_get_stats(bridge->contexts[i].ctx,
                              &weight_updates, &training_steps, &total_delta);

        bridge->metrics.synapses_updated += weight_updates;
    }

    /* Compute averages if we have contexts */
    if (active_count > 0) {
        bridge->metrics.firing_rate_mean = total_firing_rate / active_count;
        bridge->metrics.weight_mean = total_weight_mean / active_count;
    }

    /* Compute LTP/LTD ratio */
    uint64_t total_events = bridge->metrics.ltp_count + bridge->metrics.ltd_count;
    if (total_events > 0) {
        bridge->metrics.ltp_ltd_ratio =
            (float)bridge->metrics.ltp_count / (float)total_events;
    } else {
        bridge->metrics.ltp_ltd_ratio = 0.5f;  /* Neutral if no events */
    }

    /* Compute averages */
    if (bridge->metrics.ltp_count > 0) {
        bridge->metrics.avg_ltp_magnitude =
            bridge->metrics.total_delta_w / bridge->metrics.ltp_count;
    }

    /* Compute stability */
    bridge->metrics.learning_stability = compute_learning_stability(&bridge->metrics);

    /* Add reward metrics */
    bridge->metrics.reward_cumulative = bridge->reward_cumulative;
    for (int i = 0; i < SNN_REWARD_SOURCE_COUNT; i++) {
        bridge->metrics.reward_current += bridge->reward_by_source[i];
    }

    /* Mark as valid */
    bridge->metrics.valid = true;
    bridge->metrics.epoch = bridge->state.epoch;
    bridge->metrics.step = bridge->state.step;
}

//=============================================================================
// Lifecycle Implementation
//=============================================================================

void snn_training_integration_config_default(
    snn_training_integration_config_t* config
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_config_default: null config pointer");
        return;
    }

    memset(config, 0, sizeof(snn_training_integration_config_t));

    config->op_mode = SNN_TRAINING_INTEGRATION_OP_AUTOMATIC;
    config->learning_mode = SNN_TRAINING_INTEGRATION_MODE_STDP;

    config->update_interval_ms = SNN_TRAINING_INTEGRATION_DEFAULT_UPDATE_INTERVAL_MS;
    config->history_length = 100;
    config->compute_entropy = true;
    config->track_ltp_ltd_balance = true;

    config->stdp_modulation_scale = SNN_TRAINING_INTEGRATION_DEFAULT_STDP_MODULATION_SCALE;
    config->reward_modulation_scale = 1.0f;
    config->tau_modulation_scale = 1.0f;

    config->enable_loss_reward = true;
    config->loss_reward_sensitivity = 1.0f;
    config->enable_curiosity_reward = true;
    config->curiosity_reward_weight = 0.3f;
    config->enable_novelty_reward = true;
    config->novelty_reward_weight = 0.2f;

    config->stability_threshold = 0.3f;
    config->saturation_warning_ratio = 0.1f;
    config->enable_emergency_brake = true;
    config->emergency_instability_threshold = 0.2f;

    config->enable_cognitive_training = true;
    config->enable_training_immune = true;
    config->enable_training_plasticity = true;
    config->enable_bio_async = true;

    config->auto_update = true;
    config->report_interval_steps = 100;
}

snn_training_integration_bridge_t* snn_training_integration_create(
    const snn_training_integration_config_t* config
) {
    snn_training_integration_bridge_t* bridge = nimcp_malloc(sizeof(*bridge));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN training integration bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_training_integration_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(*bridge));

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(snn_training_integration_config_t));
    } else {
        snn_training_integration_config_default(&bridge->config);
    }

    /* Initialize mutex */
    if (bridge_base_init(&bridge->base, 0, "snn_training_integration") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_training_integration_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for SNN training integration bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_training_integration_create: mutex creation failed");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize state */
    bridge->state.op_mode = bridge->config.op_mode;
    bridge->state.learning_mode = bridge->config.learning_mode;
    bridge->state.current_lr_factor = 1.0f;
    bridge->state.current_reward_scale = SNN_TRAINING_INTEGRATION_DEFAULT_REWARD_SCALE;
    bridge->state.learning_stability = 1.0f;

    /* Initialize params with defaults */
    bridge->params.lr_factor = 1.0f;
    bridge->params.stdp_amplitude_scale = 1.0f;
    bridge->params.eligibility_decay_scale = 1.0f;
    bridge->params.reward_scale = SNN_TRAINING_INTEGRATION_DEFAULT_REWARD_SCALE;
    for (int i = 0; i < SNN_REWARD_SOURCE_COUNT; i++) {
        bridge->params.reward_source_weights[i] = 1.0f;
    }
    bridge->params.tau_plus_scale = 1.0f;
    bridge->params.tau_minus_scale = 1.0f;
    bridge->params.eligibility_tau_scale = 1.0f;
    bridge->params.target_rate_scale = 1.0f;
    bridge->params.homeostatic_rate_scale = 1.0f;
    bridge->params.weight_clip_scale = 1.0f;
    bridge->params.gradient_clip = 10.0f;
    bridge->params.valid = true;

    NIMCP_LOGGING_INFO("Created SNN training integration bridge");
    return bridge;
}

void snn_training_integration_destroy(
    snn_training_integration_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_destroy: null bridge pointer");
        return;
    }

    /* Stop if running */
    if (bridge->started) {
        snn_training_integration_stop(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN training integration bridge");
}

int snn_training_integration_start(
    snn_training_integration_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_start: null bridge pointer");
        return -1;
    }
    if (bridge->started) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Connect bio-async if enabled */
    if (bridge->config.enable_bio_async) {
        snn_training_integration_connect_bio_async(bridge);
    }

    bridge->started = true;
    bridge->state.timestamp_ms = 0;  /* Will be set on first update */

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Started SNN training integration bridge");
    return 0;
}

int snn_training_integration_stop(
    snn_training_integration_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_stop: null bridge pointer");
        return -1;
    }
    if (!bridge->started) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        snn_training_integration_disconnect_bio_async(bridge);
    }

    bridge->started = false;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Stopped SNN training integration bridge");
    return 0;
}

//=============================================================================
// SNN Context Connection Implementation
//=============================================================================

int snn_training_integration_connect_context(
    snn_training_integration_bridge_t* bridge,
    snn_training_ctx_t* ctx,
    snn_network_t* network,
    const char* name
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_connect_context: null bridge pointer");
        return -1;
    }
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_connect_context: null ctx pointer");
        return -1;
    }
    if (bridge->num_contexts >= SNN_TRAINING_INTEGRATION_MAX_CONTEXTS) return -2;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Find free slot */
    int slot = -1;
    for (uint32_t i = 0; i < SNN_TRAINING_INTEGRATION_MAX_CONTEXTS; i++) {
        if (!bridge->contexts[i].active) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return -2;
    }

    /* Initialize entry */
    bridge->contexts[slot].ctx = ctx;
    bridge->contexts[slot].network = network;
    bridge->contexts[slot].active = true;
    bridge->contexts[slot].ltp_count = 0;
    bridge->contexts[slot].ltd_count = 0;
    bridge->contexts[slot].total_delta_w = 0.0f;

    if (name) {
        strncpy(bridge->contexts[slot].name, name,
                sizeof(bridge->contexts[slot].name) - 1);
    } else {
        snprintf(bridge->contexts[slot].name,
                sizeof(bridge->contexts[slot].name),
                "context_%d", slot);
    }

    bridge->num_contexts++;
    bridge->state.contexts_connected = bridge->num_contexts;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected SNN context '%s' (slot %d)",
                      bridge->contexts[slot].name, slot);
    return slot;
}

int snn_training_integration_disconnect_context(
    snn_training_integration_bridge_t* bridge,
    int context_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_disconnect_context: null bridge pointer");
        return -1;
    }
    if (context_id < 0 || context_id >= SNN_TRAINING_INTEGRATION_MAX_CONTEXTS) return -1;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (!bridge->contexts[context_id].active) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_disconnect_context: bridge->contexts is NULL");
        return -1;
    }

    bridge->contexts[context_id].active = false;
    bridge->contexts[context_id].ctx = NULL;
    bridge->contexts[context_id].network = NULL;
    bridge->num_contexts--;
    bridge->state.contexts_connected = bridge->num_contexts;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected SNN context (slot %d)", context_id);
    return 0;
}

//=============================================================================
// Training Pipeline Connection Implementation
//=============================================================================

int snn_training_integration_connect_brain_training(
    snn_training_integration_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training_ctx
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_connect_brain_training: null bridge pointer");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->brain_training = training_ctx;
    bridge->stats.brain_training_connected = (training_ctx != NULL);
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected brain training context: %s",
                      training_ctx ? "yes" : "disconnected");
    return 0;
}

int snn_training_integration_connect_cognitive_training(
    snn_training_integration_bridge_t* bridge,
    cognitive_training_bridge_t* cognitive_training
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_connect_cognitive_training: null bridge pointer");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->cognitive_training = cognitive_training;
    bridge->stats.cognitive_training_connected = (cognitive_training != NULL);
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected cognitive training bridge: %s",
                      cognitive_training ? "yes" : "disconnected");
    return 0;
}

int snn_training_integration_connect_training_immune(
    snn_training_integration_bridge_t* bridge,
    training_immune_system_t* training_immune
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_connect_training_immune: null bridge pointer");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->training_immune = training_immune;
    bridge->stats.training_immune_connected = (training_immune != NULL);
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected training immune system: %s",
                      training_immune ? "yes" : "disconnected");
    return 0;
}

int snn_training_integration_connect_training_plasticity(
    snn_training_integration_bridge_t* bridge,
    training_plasticity_bridge_t* training_plasticity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_connect_training_plasticity: null bridge pointer");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->training_plasticity = training_plasticity;
    bridge->stats.training_plasticity_connected = (training_plasticity != NULL);
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected training plasticity bridge: %s",
                      training_plasticity ? "yes" : "disconnected");
    return 0;
}

//=============================================================================
// Metrics API Implementation
//=============================================================================

int snn_training_integration_get_metrics(
    const snn_training_integration_bridge_t* bridge,
    snn_training_metrics_t* metrics
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_get_metrics: null bridge pointer");
        return -1;
    }
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_get_metrics: null metrics pointer");
        return -1;
    }

    nimcp_platform_mutex_lock(((snn_training_integration_bridge_t*)bridge)->mutex);
    memcpy(metrics, &bridge->metrics, sizeof(snn_training_metrics_t));
    nimcp_platform_mutex_unlock(((snn_training_integration_bridge_t*)bridge)->mutex);

    return 0;
}

float snn_training_integration_get_ltp_ltd_ratio(
    const snn_training_integration_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_get_ltp_ltd_ratio: null bridge pointer");
        return 0.5f;
    }
    return bridge->metrics.ltp_ltd_ratio;
}

float snn_training_integration_get_learning_stability(
    const snn_training_integration_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_get_learning_stability: null bridge pointer");
        return 0.0f;
    }
    return bridge->metrics.learning_stability;
}

float snn_training_integration_get_cumulative_reward(
    const snn_training_integration_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_get_cumulative_reward: null bridge pointer");
        return 0.0f;
    }
    return bridge->reward_cumulative;
}

float snn_training_integration_get_eligibility_mean(
    const snn_training_integration_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_get_eligibility_mean: null bridge pointer");
        return 0.0f;
    }
    return bridge->metrics.eligibility_mean;
}

//=============================================================================
// Parameter API Implementation
//=============================================================================

int snn_training_integration_set_params(
    snn_training_integration_bridge_t* bridge,
    const snn_pipeline_params_t* params
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_set_params: null bridge pointer");
        return -1;
    }
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_set_params: null params pointer");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(&bridge->params, params, sizeof(snn_pipeline_params_t));
    bridge->params.valid = true;

    /* Update state */
    bridge->state.current_lr_factor = params->lr_factor;
    bridge->state.current_reward_scale = params->reward_scale;
    bridge->learning_paused = params->pause_learning;
    bridge->consolidation_active = params->consolidation_mode;
    bridge->exploration_active = params->exploration_mode;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    bridge->stats.lr_modulations++;
    return 0;
}

int snn_training_integration_apply_lr_modulation(
    snn_training_integration_bridge_t* bridge,
    float lr_factor
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_apply_lr_modulation: null bridge pointer");
        return -1;
    }
    if (lr_factor < 0.1f) lr_factor = 0.1f;
    if (lr_factor > 2.0f) lr_factor = 2.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->params.lr_factor = lr_factor;
    bridge->params.stdp_amplitude_scale = 1.0f +
        (lr_factor - 1.0f) * bridge->config.stdp_modulation_scale;
    bridge->state.current_lr_factor = lr_factor;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    bridge->stats.lr_modulations++;
    return 0;
}

int snn_training_integration_set_reward(
    snn_training_integration_bridge_t* bridge,
    float reward,
    snn_reward_source_t source
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_set_reward: null bridge pointer");
        return -1;
    }
    if (source < 0 || source >= SNN_REWARD_SOURCE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_training_integration_set_reward: invalid reward source");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Apply source weight */
    float weighted_reward = reward * bridge->params.reward_source_weights[source];
    bridge->reward_by_source[source] = weighted_reward;
    bridge->reward_cumulative += weighted_reward;

    /* Update metrics */
    bridge->metrics.reward_current = 0.0f;
    for (int i = 0; i < SNN_REWARD_SOURCE_COUNT; i++) {
        bridge->metrics.reward_current += bridge->reward_by_source[i];
    }
    bridge->metrics.reward_cumulative = bridge->reward_cumulative;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    bridge->stats.total_reward_events++;
    return 0;
}

int snn_training_integration_pause_learning(
    snn_training_integration_bridge_t* bridge,
    bool pause
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_pause_learning: null bridge pointer");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->learning_paused = pause;
    bridge->params.pause_learning = pause;
    bridge->state.learning_paused = pause;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    if (pause) {
        bridge->stats.learning_pauses++;
        NIMCP_LOGGING_INFO("SNN learning paused");
    } else {
        NIMCP_LOGGING_INFO("SNN learning resumed");
    }
    return 0;
}

int snn_training_integration_consolidation_mode(
    snn_training_integration_bridge_t* bridge,
    bool enable
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_consolidation_mode: null bridge pointer");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->consolidation_active = enable;
    bridge->params.consolidation_mode = enable;
    bridge->state.consolidation_active = enable;

    /* Adjust parameters for consolidation */
    if (enable) {
        bridge->params.stdp_amplitude_scale *= 0.3f;  /* Reduce new learning */
        bridge->params.eligibility_decay_scale *= 1.5f;  /* Faster decay */
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("SNN consolidation mode: %s", enable ? "enabled" : "disabled");
    return 0;
}

int snn_training_integration_exploration_mode(
    snn_training_integration_bridge_t* bridge,
    bool enable
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_exploration_mode: null bridge pointer");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->exploration_active = enable;
    bridge->params.exploration_mode = enable;
    bridge->state.exploration_active = enable;

    /* Adjust parameters for exploration */
    if (enable) {
        bridge->params.tau_plus_scale *= 1.2f;   /* Widen LTP window */
        bridge->params.tau_minus_scale *= 1.2f;  /* Widen LTD window */
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("SNN exploration mode: %s", enable ? "enabled" : "disabled");
    return 0;
}

//=============================================================================
// Update Cycle Implementation
//=============================================================================

int snn_training_integration_update(
    snn_training_integration_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_update: null bridge pointer");
        return -1;
    }
    if (!bridge->started) return 0;
    if (bridge->config.op_mode == SNN_TRAINING_INTEGRATION_OP_DISABLED) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Aggregate metrics from all contexts */
    aggregate_context_metrics(bridge);

    /* Update state */
    bridge->state.learning_stability = bridge->metrics.learning_stability;
    bridge->state.step++;
    bridge->state.timestamp_ms += (uint64_t)dt_ms;

    /* Check for emergency conditions */
    if (bridge->config.enable_emergency_brake &&
        bridge->metrics.learning_stability < bridge->config.emergency_instability_threshold) {
        if (!bridge->emergency_brake_active) {
            bridge->emergency_brake_active = true;
            bridge->state.emergency_brake_active = true;
            bridge->params.pause_learning = true;
            bridge->stats.emergency_brakes++;
            NIMCP_LOGGING_WARN("Emergency brake activated - stability: %.3f",
                              bridge->metrics.learning_stability);
        }
    } else if (bridge->emergency_brake_active &&
               bridge->metrics.learning_stability > bridge->config.stability_threshold) {
        /* Release emergency brake when stability recovers */
        bridge->emergency_brake_active = false;
        bridge->state.emergency_brake_active = false;
        bridge->params.pause_learning = bridge->learning_paused;  /* Restore original */
        NIMCP_LOGGING_INFO("Emergency brake released - stability: %.3f",
                          bridge->metrics.learning_stability);
    }

    /* Update statistics */
    bridge->stats.total_update_calls++;
    bridge->stats.last_update_ms = bridge->state.timestamp_ms;
    bridge->stats.snn_contexts_connected = bridge->num_contexts;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int snn_training_integration_report_event(
    snn_training_integration_bridge_t* bridge,
    snn_learning_event_t event,
    float magnitude
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_report_event: null bridge pointer");
        return -1;
    }
    if (event < 0 || event >= SNN_LEARNING_EVENT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_training_integration_report_event: invalid event type");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    switch (event) {
        case SNN_LEARNING_EVENT_LTP:
            bridge->stats.total_ltp_events++;
            bridge->metrics.ltp_count++;
            bridge->metrics.total_delta_w += fabsf(magnitude);
            break;

        case SNN_LEARNING_EVENT_LTD:
            bridge->stats.total_ltd_events++;
            bridge->metrics.ltd_count++;
            bridge->metrics.total_delta_w -= fabsf(magnitude);
            break;

        case SNN_LEARNING_EVENT_ELIGIBILITY:
            bridge->stats.total_eligibility_updates++;
            break;

        case SNN_LEARNING_EVENT_REWARD:
            bridge->stats.total_reward_events++;
            bridge->reward_cumulative += magnitude;
            break;

        case SNN_LEARNING_EVENT_HOMEOSTATIC:
            bridge->stats.total_homeostatic_adjustments++;
            bridge->metrics.homeostatic_adjustments++;
            break;

        case SNN_LEARNING_EVENT_SATURATION:
            bridge->stats.saturation_warnings++;
            break;

        case SNN_LEARNING_EVENT_INSTABILITY:
            bridge->stats.instability_events++;
            break;

        default:
            break;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int snn_training_integration_epoch_complete(
    snn_training_integration_bridge_t* bridge,
    uint64_t epoch
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_epoch_complete: null bridge pointer");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Store epoch metrics */
    bridge->state.epoch = epoch;
    bridge->metrics.epoch = epoch;
    bridge->state.step = 0;

    /* Update running averages */
    bridge->stats.avg_ltp_ltd_ratio =
        (bridge->stats.avg_ltp_ltd_ratio * 0.9f) +
        (bridge->metrics.ltp_ltd_ratio * 0.1f);
    bridge->stats.avg_eligibility =
        (bridge->stats.avg_eligibility * 0.9f) +
        (bridge->metrics.eligibility_mean * 0.1f);
    bridge->stats.avg_reward =
        (bridge->stats.avg_reward * 0.9f) +
        (bridge->metrics.reward_cumulative * 0.1f);
    bridge->stats.avg_learning_stability =
        (bridge->stats.avg_learning_stability * 0.9f) +
        (bridge->metrics.learning_stability * 0.1f);

    /* Reset per-epoch accumulators */
    bridge->reward_cumulative = 0.0f;
    for (int i = 0; i < SNN_REWARD_SOURCE_COUNT; i++) {
        bridge->reward_by_source[i] = 0.0f;
    }

    /* Reset context accumulators */
    for (uint32_t i = 0; i < bridge->num_contexts; i++) {
        if (bridge->contexts[i].active) {
            bridge->contexts[i].ltp_count = 0;
            bridge->contexts[i].ltd_count = 0;
            bridge->contexts[i].total_delta_w = 0.0f;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("SNN epoch %lu complete, stability=%.3f",
                       (unsigned long)epoch, bridge->metrics.learning_stability);
    return 0;
}

//=============================================================================
// Bio-Async Implementation
//=============================================================================

int snn_training_integration_connect_bio_async(
    snn_training_integration_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Bio-async router registration would happen here */
    /* For now, mark as not available (common in unit tests) */
    bridge->base.bio_async_enabled = false;
    bridge->stats.bio_async_connected = false;

    NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    return 0;
}

int snn_training_integration_disconnect_bio_async(
    snn_training_integration_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_disconnect_bio_async: null bridge pointer");
        return -1;
    }

    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;
    bridge->stats.bio_async_connected = false;

    return 0;
}

bool snn_training_integration_is_bio_async_connected(
    const snn_training_integration_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// State and Statistics Implementation
//=============================================================================

int snn_training_integration_get_state(
    const snn_training_integration_bridge_t* bridge,
    snn_training_integration_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_get_state: null bridge pointer");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_get_state: null state pointer");
        return -1;
    }

    nimcp_platform_mutex_lock(((snn_training_integration_bridge_t*)bridge)->mutex);
    memcpy(state, &bridge->state, sizeof(snn_training_integration_state_t));
    nimcp_platform_mutex_unlock(((snn_training_integration_bridge_t*)bridge)->mutex);

    return 0;
}

int snn_training_integration_get_stats(
    const snn_training_integration_bridge_t* bridge,
    snn_training_integration_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_get_stats: null bridge pointer");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_get_stats: null stats pointer");
        return -1;
    }

    nimcp_platform_mutex_lock(((snn_training_integration_bridge_t*)bridge)->mutex);
    memcpy(stats, &bridge->stats, sizeof(snn_training_integration_stats_t));
    nimcp_platform_mutex_unlock(((snn_training_integration_bridge_t*)bridge)->mutex);

    return 0;
}

void snn_training_integration_reset_stats(
    snn_training_integration_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_integration_reset_stats: null bridge pointer");
        return;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Preserve connection status */
    bool brain_conn = bridge->stats.brain_training_connected;
    bool cog_conn = bridge->stats.cognitive_training_connected;
    bool imm_conn = bridge->stats.training_immune_connected;
    bool plas_conn = bridge->stats.training_plasticity_connected;
    bool bio_conn = bridge->stats.bio_async_connected;
    uint32_t ctx_count = bridge->stats.snn_contexts_connected;

    memset(&bridge->stats, 0, sizeof(snn_training_integration_stats_t));

    /* Restore connection status */
    bridge->stats.brain_training_connected = brain_conn;
    bridge->stats.cognitive_training_connected = cog_conn;
    bridge->stats.training_immune_connected = imm_conn;
    bridge->stats.training_plasticity_connected = plas_conn;
    bridge->stats.bio_async_connected = bio_conn;
    bridge->stats.snn_contexts_connected = ctx_count;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// Utility Implementation
//=============================================================================

const char* snn_training_integration_mode_to_string(
    snn_training_integration_mode_t mode
) {
    switch (mode) {
        case SNN_TRAINING_INTEGRATION_MODE_STDP:       return "STDP";
        case SNN_TRAINING_INTEGRATION_MODE_RSTDP:      return "R-STDP";
        case SNN_TRAINING_INTEGRATION_MODE_EPROP:      return "eProp";
        case SNN_TRAINING_INTEGRATION_MODE_SURROGATE:  return "Surrogate";
        case SNN_TRAINING_INTEGRATION_MODE_HOMEOSTATIC: return "Homeostatic";
        case SNN_TRAINING_INTEGRATION_MODE_HYBRID:     return "Hybrid";
        default:                                        return "Unknown";
    }
}

const char* snn_training_integration_reward_source_to_string(
    snn_reward_source_t source
) {
    switch (source) {
        case SNN_REWARD_SOURCE_EXTERNAL:   return "External";
        case SNN_REWARD_SOURCE_LOSS:       return "Loss";
        case SNN_REWARD_SOURCE_CURIOSITY:  return "Curiosity";
        case SNN_REWARD_SOURCE_EMOTION:    return "Emotion";
        case SNN_REWARD_SOURCE_COGNITIVE:  return "Cognitive";
        case SNN_REWARD_SOURCE_NOVELTY:    return "Novelty";
        case SNN_REWARD_SOURCE_PREDICTION: return "Prediction";
        default:                            return "Unknown";
    }
}

const char* snn_training_integration_event_to_string(
    snn_learning_event_t event
) {
    switch (event) {
        case SNN_LEARNING_EVENT_LTP:          return "LTP";
        case SNN_LEARNING_EVENT_LTD:          return "LTD";
        case SNN_LEARNING_EVENT_ELIGIBILITY:  return "Eligibility";
        case SNN_LEARNING_EVENT_REWARD:       return "Reward";
        case SNN_LEARNING_EVENT_HOMEOSTATIC:  return "Homeostatic";
        case SNN_LEARNING_EVENT_SATURATION:   return "Saturation";
        case SNN_LEARNING_EVENT_INSTABILITY:  return "Instability";
        default:                               return "Unknown";
    }
}
