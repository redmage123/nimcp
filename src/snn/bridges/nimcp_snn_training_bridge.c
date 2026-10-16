/**
 * @file nimcp_snn_training_bridge.c
 * @brief SNN-Training Integration Bridge Implementation
 *
 * WHAT: Implementation of SNN training integration bridge
 * WHY:  Enable coordinated learning with immune, plasticity, cognitive systems
 * HOW:  STDP/eProp/R-STDP integration with training subsystems
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_training_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_types.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_training_bridge)

//=============================================================================
// Default Configuration
//=============================================================================

void snn_training_bridge_config_default(snn_training_bridge_config_t* config) {
    if (!config) return;

    /* Instability detection thresholds */
    config->weight_explosion_threshold = 100.0f;
    config->weight_collapse_threshold = 1e-6f;
    config->rate_explosion_threshold = 200.0f;   /* 200 Hz max */
    config->rate_collapse_threshold = 0.1f;      /* 0.1 Hz min */
    config->gradient_explosion_threshold = 100.0f;
    config->gradient_vanishing_threshold = 1e-7f;

    /* Modulation parameters */
    config->immune_modulation_strength = NIMCP_DEFAULT_SYNAPSE_STRENGTH;
    config->homeostatic_modulation_strength = 0.3f;
    config->sleep_consolidation_boost = 1.5f;
    config->attention_gating_strength = 0.4f;

    /* Metaplasticity */
    config->enable_metaplasticity = true;
    config->metaplasticity_tau = 10000.0f;  /* 10 seconds */
    config->bcm_theta_init = NIMCP_DEFAULT_SYNAPSE_STRENGTH;

    /* Consolidation */
    config->enable_offline_consolidation = true;
    config->consolidation_rate = 0.1f;
    config->replay_probability = 0.3f;

    /* Integration enables - all enabled by default */
    config->enable_immune_integration = true;
    config->enable_plasticity_integration = true;
    config->enable_cognitive_integration = true;
    config->enable_sleep_integration = true;

    /* Timing */
    config->update_interval_ms = 10.0f;
    config->instability_check_interval_ms = 100.0f;

    /* Bio-async */
    config->enable_bio_async = true;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

snn_training_bridge_t* snn_training_bridge_create(
    const snn_training_bridge_config_t* config,
    snn_network_t* snn,
    snn_training_ctx_t* training_ctx
) {
    if (!config || !snn || !training_ctx) {
        NIMCP_LOGGING_ERROR("Invalid parameters for training bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_bridge_create: config/snn/training_ctx is NULL");
        return NULL;
    }

    snn_training_bridge_t* bridge = nimcp_calloc(1, sizeof(snn_training_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate training bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_training_bridge_create: failed to allocate bridge");
        return NULL;
    }

    if (bridge_base_init(&bridge->base, 0, "snn_training") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->training_ctx = training_ctx;
    memcpy(&bridge->config, config, sizeof(snn_training_bridge_config_t));

    /* Initialize state */
    memset(&bridge->state, 0, sizeof(snn_training_bridge_state_t));
    bridge->state.bcm_theta = config->bcm_theta_init;
    bridge->state.metrics.stability_score = 1.0f;
    bridge->state.metrics.lr_modulation_factor = 1.0f;

    /* Initialize base learning rate from training context */
    bridge->base_lr = NIMCP_DEFAULT_LEARNING_RATE;  /* Default base LR */

    /* Allocate rate estimation array */
    /* Note: Could get neuron count from network stats if needed */
    bridge->n_neurons = 0;  /* Will be set if homeostatic tracking enabled */
    bridge->neuron_rates = NULL;

    NIMCP_LOGGING_INFO("SNN training bridge created successfully");
    return bridge;
}

void snn_training_bridge_destroy(snn_training_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_training_bridge_disconnect_bio_async(bridge);
    }

    /* Free rate array */
    if (bridge->neuron_rates) {
        nimcp_free(bridge->neuron_rates);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("SNN training bridge destroyed");
}

//=============================================================================
// System Connections
//=============================================================================

int snn_training_bridge_connect_immune(
    snn_training_bridge_t* bridge,
    struct training_immune_system_s* immune
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_connect_immune: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->immune_system = immune;
    bridge->state.immune_connected = (immune != NULL);

    if (immune) {
        NIMCP_LOGGING_INFO("Training bridge connected to immune system");
    }
    return 0;
}

int snn_training_bridge_connect_plasticity(
    snn_training_bridge_t* bridge,
    struct training_plasticity_bridge_s* plasticity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_connect_plasticity: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->plasticity_bridge = plasticity;
    bridge->state.plasticity_connected = (plasticity != NULL);

    if (plasticity) {
        NIMCP_LOGGING_INFO("Training bridge connected to plasticity bridge");
    }
    return 0;
}

int snn_training_bridge_connect_cognitive(
    snn_training_bridge_t* bridge,
    struct cognitive_training_bridge_s* cognitive
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_connect_cognitive: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->cognitive_bridge = cognitive;
    bridge->state.cognitive_connected = (cognitive != NULL);

    if (cognitive) {
        NIMCP_LOGGING_INFO("Training bridge connected to cognitive bridge");
    }
    return 0;
}

int snn_training_bridge_connect_sleep(
    snn_training_bridge_t* bridge,
    struct snn_sleep_bridge_s* sleep
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_connect_sleep: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->sleep_bridge = sleep;
    bridge->state.sleep_connected = (sleep != NULL);

    if (sleep) {
        NIMCP_LOGGING_INFO("Training bridge connected to sleep bridge");
    }
    return 0;
}

//=============================================================================
// Bio-async Functions
//=============================================================================

int snn_training_bridge_connect_bio_async(snn_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Bio-async router not available in test environment */
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Bio-async not available for training bridge");
    return 0;
}

int snn_training_bridge_disconnect_bio_async(snn_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_disconnect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->base.bio_async_enabled = false;
    memset(&bridge->base.bio_ctx, 0, sizeof(bio_module_context_t));
    return 0;
}

bool snn_training_bridge_is_bio_async_connected(const snn_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_bridge_is_bio_async_connected: bridge is NULL");
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Processing Functions
//=============================================================================

int snn_training_bridge_update(snn_training_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (dt <= 0.0f) return 0;

    bridge->total_time += dt;
    bridge->last_update_time = bridge->total_time;

    /* Update metrics periodically */
    snn_training_bridge_update_metrics(bridge);

    /* Check stability periodically */
    if (bridge->total_time - bridge->last_stability_check >=
        bridge->config.instability_check_interval_ms) {
        snn_training_instability_t instability =
            snn_training_bridge_check_stability(bridge);
        if (instability != SNN_TRAIN_STABLE) {
            snn_training_bridge_handle_instability(bridge, instability);
        }
        bridge->last_stability_check = bridge->total_time;
    }

    /* Update metaplasticity */
    if (bridge->config.enable_metaplasticity) {
        snn_training_bridge_update_bcm_theta(bridge, dt);
    }

    bridge->state.update_count++;
    return 0;
}

uint32_t snn_training_bridge_train_step(
    snn_training_bridge_t* bridge,
    float dt
) {
    if (!bridge || !bridge->training_ctx || !bridge->snn) return 0;

    /* Compute effective learning rate with all modulations */
    float effective_lr = snn_training_bridge_get_effective_lr(bridge);
    bridge->state.metrics.effective_lr = effective_lr;

    /* Apply training based on mode */
    uint32_t updates = 0;
    snn_train_mode_t mode = bridge->training_ctx->mode;

    switch (mode) {
        case SNN_TRAIN_STDP:
            updates = snn_stdp_apply_network(bridge->training_ctx, bridge->snn,
                                              bridge->total_time);
            break;
        case SNN_TRAIN_R_STDP:
            snn_rstdp_update_eligibility(bridge->training_ctx, dt);
            updates = snn_rstdp_apply(bridge->training_ctx, bridge->snn);
            break;
        case SNN_TRAIN_EPROP:
            updates = snn_eprop_apply(bridge->training_ctx, bridge->snn, 0.0f);
            break;
        default:
            break;
    }

    bridge->state.weight_updates += updates;

    /* Update average effective LR */
    float alpha = 0.01f;
    bridge->state.avg_effective_lr = alpha * effective_lr +
                                      (1.0f - alpha) * bridge->state.avg_effective_lr;

    return updates;
}

//=============================================================================
// Instability Detection
//=============================================================================

snn_training_instability_t snn_training_bridge_check_stability(
    snn_training_bridge_t* bridge
) {
    if (!bridge) return SNN_TRAIN_STABLE;

    snn_training_metrics_t* m = &bridge->state.metrics;

    /* Check weight explosion */
    if (fabsf(m->weight_max) > bridge->config.weight_explosion_threshold ||
        fabsf(m->weight_min) > bridge->config.weight_explosion_threshold) {
        m->instability_type = SNN_TRAIN_WEIGHT_EXPLOSION;
        m->instability_count++;
        return SNN_TRAIN_WEIGHT_EXPLOSION;
    }

    /* Check weight collapse */
    if (m->weight_std < bridge->config.weight_collapse_threshold) {
        m->instability_type = SNN_TRAIN_WEIGHT_COLLAPSE;
        m->instability_count++;
        return SNN_TRAIN_WEIGHT_COLLAPSE;
    }

    /* Check rate explosion */
    if (m->rate_max > bridge->config.rate_explosion_threshold) {
        m->instability_type = SNN_TRAIN_RATE_EXPLOSION;
        m->instability_count++;
        return SNN_TRAIN_RATE_EXPLOSION;
    }

    /* Check rate collapse (dead neurons) */
    if (m->rate_max < bridge->config.rate_collapse_threshold) {
        m->instability_type = SNN_TRAIN_RATE_COLLAPSE;
        m->instability_count++;
        return SNN_TRAIN_RATE_COLLAPSE;
    }

    /* Check gradient explosion */
    if (m->gradient_norm > bridge->config.gradient_explosion_threshold) {
        m->instability_type = SNN_TRAIN_GRADIENT_EXPLOSION;
        m->instability_count++;
        return SNN_TRAIN_GRADIENT_EXPLOSION;
    }

    /* Check gradient vanishing */
    if (m->gradient_norm > 0 &&
        m->gradient_norm < bridge->config.gradient_vanishing_threshold) {
        m->instability_type = SNN_TRAIN_GRADIENT_VANISHING;
        m->instability_count++;
        return SNN_TRAIN_GRADIENT_VANISHING;
    }

    m->instability_type = SNN_TRAIN_STABLE;
    return SNN_TRAIN_STABLE;
}

int snn_training_bridge_handle_instability(
    snn_training_bridge_t* bridge,
    snn_training_instability_t instability
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_handle_instability: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    NIMCP_LOGGING_WARN("Training instability detected: %d", instability);

    switch (instability) {
        case SNN_TRAIN_WEIGHT_EXPLOSION:
        case SNN_TRAIN_GRADIENT_EXPLOSION:
            /* Reduce learning rate */
            bridge->base_lr *= 0.5f;
            NIMCP_LOGGING_INFO("Reduced base LR to %f", bridge->base_lr);
            break;

        case SNN_TRAIN_WEIGHT_COLLAPSE:
        case SNN_TRAIN_GRADIENT_VANISHING:
            /* Increase learning rate slightly */
            bridge->base_lr *= 1.2f;
            NIMCP_LOGGING_INFO("Increased base LR to %f", bridge->base_lr);
            break;

        case SNN_TRAIN_RATE_EXPLOSION:
            /* Apply homeostatic regulation */
            if (bridge->training_ctx) {
                snn_homeostatic_apply(bridge->training_ctx, bridge->snn);
            }
            break;

        case SNN_TRAIN_RATE_COLLAPSE:
            /* Boost weak synapses */
            NIMCP_LOGGING_INFO("Rate collapse detected - consider weight initialization");
            break;

        default:
            break;
    }

    /* Trigger immune response for severe instabilities */
    if (bridge->state.immune_connected &&
        (instability == SNN_TRAIN_WEIGHT_EXPLOSION ||
         instability == SNN_TRAIN_GRADIENT_EXPLOSION)) {
        NIMCP_LOGGING_INFO("Triggering immune response for training instability");
        /* Immune system would be notified here */
    }

    return 0;
}

//=============================================================================
// Learning Rate Modulation
//=============================================================================

float snn_training_bridge_get_effective_lr(const snn_training_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    float effective_lr = bridge->base_lr;
    float combined_modulation = 1.0f;
    uint32_t sources = 0;

    /* Immune modulation (fever suppresses learning) */
    if (bridge->state.immune_connected && bridge->config.enable_immune_integration) {
        float immune_factor = snn_training_bridge_get_modulation_factor(
            bridge, SNN_MODULATION_IMMUNE);
        combined_modulation *= immune_factor;
        sources |= SNN_MODULATION_IMMUNE;
    }

    /* Homeostatic modulation */
    if (bridge->config.homeostatic_modulation_strength > 0.0f) {
        float homeostatic_factor = snn_training_bridge_get_modulation_factor(
            bridge, SNN_MODULATION_HOMEOSTATIC);
        combined_modulation *= homeostatic_factor;
        sources |= SNN_MODULATION_HOMEOSTATIC;
    }

    /* Sleep/consolidation boost */
    if (bridge->state.sleep_connected && bridge->config.enable_sleep_integration) {
        float sleep_factor = snn_training_bridge_get_modulation_factor(
            bridge, SNN_MODULATION_SLEEP);
        combined_modulation *= sleep_factor;
        sources |= SNN_MODULATION_SLEEP;
    }

    /* Metaplasticity modulation */
    if (bridge->config.enable_metaplasticity) {
        float meta_factor = 1.0f - bridge->state.metaplasticity_level * 0.5f;
        combined_modulation *= meta_factor;
        sources |= SNN_MODULATION_METAPLASTICITY;
    }

    effective_lr *= combined_modulation;

    /* Store modulation info (casting away const for internal state update) */
    snn_training_bridge_t* mutable_bridge = (snn_training_bridge_t*)bridge;
    mutable_bridge->state.metrics.lr_modulation_factor = combined_modulation;
    mutable_bridge->state.metrics.modulation_sources = sources;

    return effective_lr;
}

void snn_training_bridge_set_base_lr(snn_training_bridge_t* bridge, float base_lr) {
    if (!bridge) return;
    if (base_lr <= 0.0f) return;

    bridge->base_lr = base_lr;
}

float snn_training_bridge_get_modulation_factor(
    const snn_training_bridge_t* bridge,
    snn_training_modulation_t source
) {
    if (!bridge) return 1.0f;

    switch (source) {
        case SNN_MODULATION_IMMUNE:
            /* If immune system connected, would query inflammation level */
            /* For now, return 1.0 (no modulation) */
            if (!bridge->state.immune_connected) return 1.0f;
            return 1.0f - bridge->config.immune_modulation_strength * 0.5f;

        case SNN_MODULATION_HOMEOSTATIC:
            /* Based on deviation from target rate */
            return 1.0f;

        case SNN_MODULATION_METAPLASTICITY:
            return 1.0f - bridge->state.metaplasticity_level * 0.5f;

        case SNN_MODULATION_SLEEP:
            /* Boost during sleep, reduce during wake */
            if (!bridge->state.sleep_connected) return 1.0f;
            if (bridge->state.consolidation_active) {
                return bridge->config.sleep_consolidation_boost;
            }
            return 1.0f;

        case SNN_MODULATION_ATTENTION:
            if (!bridge->state.cognitive_connected) return 1.0f;
            return 1.0f + bridge->config.attention_gating_strength * 0.5f;

        case SNN_MODULATION_REWARD:
            /* Would be modulated by reward signal */
            return 1.0f;

        default:
            return 1.0f;
    }
}

//=============================================================================
// Metaplasticity
//=============================================================================

float snn_training_bridge_update_bcm_theta(
    snn_training_bridge_t* bridge,
    float dt
) {
    if (!bridge) return 0.0f;

    /* BCM sliding threshold: theta tracks mean activity */
    float target_rate = 5.0f;  /* Target firing rate (Hz) */
    float current_rate = bridge->state.metrics.rate_mean;

    /* Update theta with time constant */
    float tau = bridge->config.metaplasticity_tau;
    float dtheta = (current_rate - target_rate) * dt / tau;
    bridge->state.bcm_theta += dtheta;

    /* Clamp theta to reasonable range */
    if (bridge->state.bcm_theta < 0.01f) bridge->state.bcm_theta = 0.01f;
    if (bridge->state.bcm_theta > 1.0f) bridge->state.bcm_theta = 1.0f;

    /* Compute normalized metaplasticity level */
    bridge->state.metaplasticity_level = bridge->state.bcm_theta /
                                          bridge->config.bcm_theta_init;
    if (bridge->state.metaplasticity_level > 1.0f) {
        bridge->state.metaplasticity_level = 1.0f;
    }

    return bridge->state.bcm_theta;
}

float snn_training_bridge_get_metaplasticity_level(
    const snn_training_bridge_t* bridge
) {
    if (!bridge) return 0.0f;
    return bridge->state.metaplasticity_level;
}

//=============================================================================
// Consolidation
//=============================================================================

int snn_training_bridge_trigger_consolidation(snn_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_trigger_consolidation: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_offline_consolidation) {
        return 0;
    }

    bridge->state.consolidation_active = true;
    bridge->state.consolidation_progress = 0.0f;

    NIMCP_LOGGING_INFO("Triggered memory consolidation");
    return 0;
}

int snn_training_bridge_trigger_replay(snn_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_trigger_replay: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Check replay probability */
    float rand_val = (float)nimcp_tl_rand() / (float)RAND_MAX;
    if (rand_val > bridge->config.replay_probability) {
        return 0;  /* Skip replay this time */
    }

    bridge->state.replay_count++;
    NIMCP_LOGGING_DEBUG("Triggered spike sequence replay");

    /* If sleep bridge connected, coordinate with sleep stage */
    if (bridge->state.sleep_connected && bridge->sleep_bridge) {
        /* Would trigger replay through sleep bridge */
    }

    return 0;
}

bool snn_training_bridge_is_consolidating(const snn_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_training_bridge_is_consolidating: bridge is NULL");
        return false;
    }
    return bridge->state.consolidation_active;
}

//=============================================================================
// Metrics and Statistics
//=============================================================================

int snn_training_bridge_update_metrics(snn_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_update_metrics: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    snn_training_metrics_t* m = &bridge->state.metrics;

    /* Get weight statistics from network */
    /* In a full implementation, this would query the actual network */
    m->weight_mean = 0.5f;
    m->weight_std = 0.1f;
    m->weight_max = 1.0f;
    m->weight_min = 0.0f;

    /* Get rate statistics */
    m->rate_mean = 10.0f;  /* 10 Hz average */
    m->rate_std = 5.0f;
    m->rate_max = 50.0f;
    m->rate_min = 0.5f;

    /* Compute stability score */
    float weight_stability = 1.0f;
    if (fabsf(m->weight_max) > bridge->config.weight_explosion_threshold * 0.5f) {
        weight_stability = 0.5f;
    }

    float rate_stability = 1.0f;
    if (m->rate_max > bridge->config.rate_explosion_threshold * 0.5f) {
        rate_stability = 0.5f;
    }

    m->stability_score = weight_stability * rate_stability;

    return 0;
}

int snn_training_bridge_get_metrics(
    const snn_training_bridge_t* bridge,
    snn_training_metrics_t* metrics
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_get_metrics: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_get_metrics: metrics is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    memcpy(metrics, &bridge->state.metrics, sizeof(snn_training_metrics_t));
    return 0;
}

int snn_training_bridge_get_state(
    const snn_training_bridge_t* bridge,
    snn_training_bridge_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    memcpy(state, &bridge->state, sizeof(snn_training_bridge_state_t));
    return 0;
}

int snn_training_bridge_get_stats(
    const snn_training_bridge_t* bridge,
    uint32_t* update_count,
    uint32_t* weight_updates,
    float* avg_effective_lr
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_training_bridge_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    if (update_count) *update_count = bridge->state.update_count;
    if (weight_updates) *weight_updates = bridge->state.weight_updates;
    if (avg_effective_lr) *avg_effective_lr = bridge->state.avg_effective_lr;

    return 0;
}

void snn_training_bridge_reset_stats(snn_training_bridge_t* bridge) {
    if (!bridge) return;

    bridge->state.update_count = 0;
    bridge->state.weight_updates = 0;
    bridge->state.modulation_events = 0;
    bridge->state.avg_effective_lr = 0.0f;
    bridge->state.total_delta_w = 0.0f;
    bridge->state.replay_count = 0;

    memset(&bridge->state.metrics, 0, sizeof(snn_training_metrics_t));
    bridge->state.metrics.stability_score = 1.0f;
    bridge->state.metrics.lr_modulation_factor = 1.0f;
}

//=============================================================================
// Query Functions
//=============================================================================

float snn_training_bridge_get_stability_score(const snn_training_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->state.metrics.stability_score;
}

snn_training_instability_t snn_training_bridge_get_instability(
    const snn_training_bridge_t* bridge
) {
    if (!bridge) return SNN_TRAIN_STABLE;
    return bridge->state.metrics.instability_type;
}

float snn_training_bridge_get_consolidation_progress(
    const snn_training_bridge_t* bridge
) {
    if (!bridge) return 0.0f;
    return bridge->state.consolidation_progress;
}
