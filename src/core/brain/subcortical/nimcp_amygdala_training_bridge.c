/**
 * @file nimcp_amygdala_training_bridge.c
 * @brief Implementation of amygdala-training integration bridge
 *
 * WHAT: Bidirectional coupling between amygdala emotional state and training
 * WHY:  Model Yerkes-Dodson arousal-learning relationship and stress responses
 * HOW:  Query amygdala state → modulate LR; detect training instability → trigger fear
 */

#include "core/brain/subcortical/nimcp_amygdala_training_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(amygdala_training_bridge, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* Alias for time function */
#define nimcp_get_current_time_ms() nimcp_time_get_ms()

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute Yerkes-Dodson LR modulation
 *
 * WHAT: Calculate inverted-U learning rate factor from arousal
 * WHY:  Implement biological arousal-performance relationship
 * HOW:  lr_factor = 1.0 - sharpness * (arousal - optimal)^2
 */
static float compute_yerkes_dodson_factor(
    float arousal,
    float optimal_arousal,
    float sharpness
) {
    float deviation = arousal - optimal_arousal;
    float factor = 1.0f - sharpness * deviation * deviation;

    /* Clamp to [0, 1] */
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;

    return factor;
}

/**
 * @brief Decay fear/anxiety over time
 *
 * WHAT: Exponential decay of emotional state
 * WHY:  Fear/anxiety naturally reduce without stimulus
 * HOW:  value *= (1.0 - decay_rate)
 */
static void decay_emotional_state(float* value, float decay_rate) {
    if (!value || *value <= 0.0f) return;

    *value *= (1.0f - decay_rate);
    if (*value < 0.01f) *value = 0.0f;  /* Floor to prevent underflow */
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int amygdala_training_default_config(amygdala_training_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Yerkes-Dodson parameters */
    config->optimal_arousal = AMYGDALA_TRAINING_OPTIMAL_AROUSAL;
    config->curve_sharpness = AMYGDALA_TRAINING_CURVE_SHARPNESS;
    config->enable_yerkes_dodson = true;

    /* Threat learning */
    config->threat_learning_boost = AMYGDALA_TRAINING_THREAT_LEARNING_BOOST;
    config->threat_fear_threshold = AMYGDALA_TRAINING_THREAT_FEAR_THRESHOLD;
    config->enable_threat_learning = true;

    /* Instability responses */
    config->fear_nan = AMYGDALA_TRAINING_FEAR_NAN;
    config->fear_inf = AMYGDALA_TRAINING_FEAR_INF;
    config->fear_explosion = AMYGDALA_TRAINING_FEAR_EXPLOSION;
    config->fear_grad_explosion = AMYGDALA_TRAINING_FEAR_GRAD_EXPLOSION;
    config->anxiety_plateau = AMYGDALA_TRAINING_ANXIETY_PLATEAU;
    config->enable_instability_response = true;

    /* State decay */
    config->fear_decay_rate = AMYGDALA_TRAINING_FEAR_DECAY_RATE;
    config->min_lr_factor = AMYGDALA_TRAINING_MIN_LR_FACTOR;

    /* Bio-async */
    config->enable_bio_async = true;
    config->bio_inbox_capacity = AMYGDALA_TRAINING_BIO_INBOX_CAPACITY;

    /* Logging */
    config->enable_logging = false;

    return 0;
}

amygdala_training_bridge_t* amygdala_training_create(
    const amygdala_training_config_t* config
) {
    /* Use defaults if no config */
    amygdala_training_config_t default_cfg;
    if (!config) {
        amygdala_training_default_config(&default_cfg);
        config = &default_cfg;
    }

    /* Allocate bridge */
    amygdala_training_bridge_t* bridge = nimcp_malloc(sizeof(amygdala_training_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate amygdala-training bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(amygdala_training_bridge_t));

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(amygdala_training_config_t));

    /* Initialize state */
    bridge->phase = AMYGDALA_TRAINING_PHASE_INACTIVE;
    bridge->arousal_level = 0.5f;  /* Start at optimal */
    bridge->fear_level = 0.0f;
    bridge->anxiety_level = 0.0f;
    bridge->lr_modulation = 1.0f;  /* Neutral */
    bridge->threat_learning_boost_active = 1.0f;

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "amygdala_training") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    /* Record creation time */
    bridge->creation_time_ms = nimcp_get_current_time_ms();
    bridge->last_update_ms = bridge->creation_time_ms;

    /* Connect to bio-async if enabled */
    if (config->enable_bio_async) {
        amygdala_training_connect_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Created amygdala-training bridge");

    return bridge;
}

void amygdala_training_destroy(amygdala_training_bridge_t* bridge) {
    if (!bridge) return;

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    NIMCP_LOGGING_INFO("Destroyed amygdala-training bridge");

    nimcp_free(bridge);
}

/* ============================================================================
 * Integration Functions
 * ============================================================================ */

int amygdala_training_connect_amygdala(
    amygdala_training_bridge_t* bridge,
    amygdala_t* amygdala
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(amygdala, NIMCP_ERROR_INVALID_PARAM, "amygdala is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->amygdala = amygdala;
    bridge->amygdala_connected = true;
    bridge->base.system_a_connected = true;

    /* Update phase if all connections ready */
    if (bridge->amygdala_connected && bridge->training_connected) {
        bridge->phase = AMYGDALA_TRAINING_PHASE_MONITORING;
        bridge->base.bridge_active = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Connected amygdala to training bridge");
    }

    return 0;
}

int amygdala_training_connect_training(
    amygdala_training_bridge_t* bridge,
    void* training
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(training, NIMCP_ERROR_INVALID_PARAM, "training is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->training_system = training;
    bridge->training_connected = true;
    bridge->base.system_b_connected = true;

    /* Update phase if all connections ready */
    if (bridge->amygdala_connected && bridge->training_connected) {
        bridge->phase = AMYGDALA_TRAINING_PHASE_MONITORING;
        bridge->base.bridge_active = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Connected training system to amygdala bridge");
    }

    return 0;
}

int amygdala_training_connect_optimizer(
    amygdala_training_bridge_t* bridge,
    nimcp_optimizer_context_t* optimizer
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(optimizer, NIMCP_ERROR_INVALID_PARAM, "optimizer is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->optimizer = optimizer;
    bridge->optimizer_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Connected optimizer to amygdala-training bridge");
    }

    return 0;
}

int amygdala_training_disconnect_amygdala(amygdala_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->amygdala = NULL;
    bridge->amygdala_connected = false;
    bridge->phase = AMYGDALA_TRAINING_PHASE_INACTIVE;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int amygdala_training_disconnect_training(amygdala_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->training_system = NULL;
    bridge->training_connected = false;
    bridge->phase = AMYGDALA_TRAINING_PHASE_INACTIVE;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int amygdala_training_disconnect_optimizer(amygdala_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->optimizer = NULL;
    bridge->optimizer_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Amygdala → Training: Arousal Modulates Learning
 * ============================================================================ */

int amygdala_training_update(amygdala_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->amygdala_connected, NIMCP_ERROR_INVALID_STATE, "amygdala not connected");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Query amygdala state */
    float fear = amygdala_get_fear_level(bridge->amygdala);
    float anxiety = amygdala_get_anxiety_level(bridge->amygdala);

    /* Compute arousal from fear + anxiety */
    float arousal = fear * AMYG_AROUSAL_FEAR_WEIGHT +
                    anxiety * AMYG_AROUSAL_ANXIETY_WEIGHT;
    if (arousal > 1.0f) arousal = 1.0f;

    /* Update internal state */
    bridge->fear_level = fear;
    bridge->anxiety_level = anxiety;
    bridge->arousal_level = arousal;

    /* Compute Yerkes-Dodson LR modulation */
    if (bridge->config.enable_yerkes_dodson) {
        bridge->lr_modulation = compute_yerkes_dodson_factor(
            arousal,
            bridge->config.optimal_arousal,
            bridge->config.curve_sharpness
        );

        /* Apply minimum LR factor */
        if (bridge->lr_modulation < bridge->config.min_lr_factor) {
            bridge->lr_modulation = bridge->config.min_lr_factor;
        }
    } else {
        bridge->lr_modulation = 1.0f;
    }

    /* Compute threat learning boost */
    if (bridge->config.enable_threat_learning &&
        fear >= bridge->config.threat_fear_threshold) {
        bridge->threat_learning_boost_active = bridge->config.threat_learning_boost;
    } else {
        bridge->threat_learning_boost_active = 1.0f;
    }

    /* Update statistics */
    bridge->total_updates++;
    bridge->base.total_updates++;
    bridge->avg_arousal = (bridge->avg_arousal * (bridge->total_updates - 1) + arousal) /
                          bridge->total_updates;
    bridge->avg_lr_modulation = (bridge->avg_lr_modulation * (bridge->total_updates - 1) +
                                  bridge->lr_modulation) / bridge->total_updates;

    /* Update phase */
    if (bridge->lr_modulation < 1.0f || bridge->threat_learning_boost_active > 1.0f) {
        bridge->phase = AMYGDALA_TRAINING_PHASE_MODULATING;
    } else {
        bridge->phase = AMYGDALA_TRAINING_PHASE_MONITORING;
    }

    bridge->last_update_ms = nimcp_get_current_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float amygdala_training_get_lr_multiplier(const amygdala_training_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->lr_modulation;
}

float amygdala_training_get_threat_boost(const amygdala_training_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->threat_learning_boost_active;
}

int amygdala_training_apply_lr_modulation(
    amygdala_training_bridge_t* bridge,
    float base_lr
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->optimizer_connected, NIMCP_ERROR_INVALID_STATE, "optimizer not connected");

    nimcp_mutex_lock(bridge->base.mutex);

    float effective_lr = base_lr * bridge->lr_modulation;

    /* Set via optimizer API (void return) */
    nimcp_optimizer_set_lr(bridge->optimizer, effective_lr);

    nimcp_mutex_unlock(bridge->base.mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Applied LR modulation: %.4f (base=%.4f, factor=%.4f)",
                          effective_lr, base_lr, bridge->lr_modulation);
    }

    return 0;
}

/* ============================================================================
 * Training → Amygdala: Instability Triggers Threat Response
 * ============================================================================ */

int amygdala_training_on_instability(
    amygdala_training_bridge_t* bridge,
    int instability_type,
    float severity
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->amygdala_connected, NIMCP_ERROR_INVALID_STATE, "amygdala not connected");
    if (!bridge->config.enable_instability_response) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    float fear_increase = 0.0f;
    float anxiety_increase = 0.0f;

    /* Map instability type to fear/anxiety response */
    switch (instability_type) {
        case TRAINING_INSTABILITY_LOSS_NAN:
        case TRAINING_INSTABILITY_LOSS_INF:
            fear_increase = bridge->config.fear_nan;
            break;

        case TRAINING_INSTABILITY_LOSS_EXPLOSION:
            fear_increase = bridge->config.fear_explosion;
            break;

        case TRAINING_INSTABILITY_GRAD_EXPLOSION:
            fear_increase = bridge->config.fear_grad_explosion;
            break;

        case TRAINING_INSTABILITY_LOSS_PLATEAU:
            anxiety_increase = bridge->config.anxiety_plateau;
            break;

        default:
            /* Unknown instability - moderate fear */
            fear_increase = 0.2f;
            break;
    }

    /* Scale by severity */
    fear_increase *= (severity / 10.0f);
    anxiety_increase *= (severity / 10.0f);

    /* Update amygdala state */
    float current_fear = amygdala_get_fear_level(bridge->amygdala);
    float current_anxiety = amygdala_get_anxiety_level(bridge->amygdala);

    float new_fear = current_fear + fear_increase;
    float new_anxiety = current_anxiety + anxiety_increase;

    /* Clamp to [0, 1] */
    if (new_fear > 1.0f) new_fear = 1.0f;
    if (new_anxiety > 1.0f) new_anxiety = 1.0f;

    /* Apply to amygdala (directly set fear/anxiety levels) */
    amygdala_set_fear_level(bridge->amygdala, new_fear);
    amygdala_set_anxiety(bridge->amygdala, new_anxiety);

    /* Update bridge state */
    bridge->instability_detected = true;
    bridge->instability_count++;
    bridge->total_instabilities++;
    bridge->last_instability_ms = nimcp_get_current_time_ms();
    bridge->phase = AMYGDALA_TRAINING_PHASE_RESPONDING;

    nimcp_mutex_unlock(bridge->base.mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_WARN("Training instability type=%d severity=%.2f → fear+=%.3f anxiety+=%.3f",
                          instability_type, severity, fear_increase, anxiety_increase);
    }

    return 0;
}

/* ============================================================================
 * Bio-async Integration
 * ============================================================================ */

int amygdala_training_connect_bio_async(amygdala_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;  /* Already connected */

    bio_module_info_t info = {
        .module_id = BIO_MODULE_AMYGDALA_TRAINING,
        .module_name = "amygdala_training_bridge",
        .inbox_capacity = bridge->config.bio_inbox_capacity,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available");
    }

    return 0;
}

int amygdala_training_disconnect_bio_async(amygdala_training_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool amygdala_training_is_bio_async_connected(const amygdala_training_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

amygdala_training_phase_t amygdala_training_get_phase(
    const amygdala_training_bridge_t* bridge
) {
    if (!bridge) return AMYGDALA_TRAINING_PHASE_INACTIVE;
    return bridge->phase;
}

float amygdala_training_get_arousal(const amygdala_training_bridge_t* bridge) {
    if (!bridge) return 0.5f;
    return bridge->arousal_level;
}

float amygdala_training_get_fear(const amygdala_training_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->fear_level;
}

float amygdala_training_get_anxiety(const amygdala_training_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->anxiety_level;
}

int amygdala_training_get_stats(
    const amygdala_training_bridge_t* bridge,
    amygdala_training_stats_t* stats
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Current state */
    stats->current_phase = bridge->phase;
    stats->current_arousal = bridge->arousal_level;
    stats->current_fear = bridge->fear_level;
    stats->current_anxiety = bridge->anxiety_level;
    stats->current_lr_modulation = bridge->lr_modulation;
    stats->current_threat_boost = bridge->threat_learning_boost_active;

    /* Counts */
    stats->total_updates = bridge->total_updates;
    stats->total_instabilities = bridge->total_instabilities;
    stats->nan_detections = 0;  /* Would need separate tracking */
    stats->inf_detections = 0;
    stats->explosion_detections = 0;
    stats->plateau_detections = 0;

    /* Averages */
    stats->avg_arousal = bridge->avg_arousal;
    stats->avg_lr_modulation = bridge->avg_lr_modulation;
    stats->min_lr_modulation = bridge->config.min_lr_factor;
    stats->max_lr_modulation = 1.0f;

    /* Connection status */
    stats->amygdala_connected = bridge->amygdala_connected;
    stats->training_connected = bridge->training_connected;
    stats->optimizer_connected = bridge->optimizer_connected;
    stats->bio_async_connected = bridge->base.bio_async_enabled;

    /* Timing */
    uint64_t now = nimcp_get_current_time_ms();
    stats->uptime_ms = now - bridge->creation_time_ms;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * String Conversion
 * ============================================================================ */

const char* amygdala_training_phase_to_string(amygdala_training_phase_t phase) {
    switch (phase) {
        case AMYGDALA_TRAINING_PHASE_INACTIVE:   return "INACTIVE";
        case AMYGDALA_TRAINING_PHASE_MONITORING: return "MONITORING";
        case AMYGDALA_TRAINING_PHASE_MODULATING: return "MODULATING";
        case AMYGDALA_TRAINING_PHASE_RESPONDING: return "RESPONDING";
        default: return "UNKNOWN";
    }
}
