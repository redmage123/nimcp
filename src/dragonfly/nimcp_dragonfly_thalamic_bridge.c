/**
 * @file nimcp_dragonfly_thalamic_bridge.c
 * @brief Implementation of Dragonfly-to-Thalamic System Bridge
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "dragonfly/nimcp_dragonfly_thalamic_bridge.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_tsdn.h"
#include "dragonfly/nimcp_dragonfly_tracking.h"
#include "core/brain/subcortical/nimcp_thalamus.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_thalamic_bridge)

#define LOG_MODULE "DRAGONFLY_THALAMIC_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_thalamic_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    bool initialized;

    /* Connected systems */
    dragonfly_system_t* dragonfly;
    thalamus_t* thalamus;

    /* Configuration */
    dragonfly_thalamic_config_t config;

    /* Current state */
    thal_routing_mode_t current_mode;

    /* Per-pathway attention levels */
    float visual_attention;
    float motor_attention;
    float attention_attention;  /* Pulvinar attention */
    float decision_attention;

    /* Per-pathway inhibition (from TRN) */
    float visual_inhibition;
    float motor_inhibition;
    float attention_inhibition;
    float decision_inhibition;

    /* Cached signals */
    thal_visual_target_t last_visual;
    thal_motor_command_t last_motor;
    thal_attention_signal_t last_attention;
    thal_decision_signal_t last_decision;
    float last_tsdn[THAL_BRIDGE_TSDN_CHANNELS];
    float last_heading;
    float last_confidence;

    /* Statistics */
    thal_bridge_stats_t stats;
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static float apply_attention_and_inhibition(float signal, float attention, float inhibition) {
    /* Signal is modulated by attention and suppressed by inhibition */
    float effective_attention = attention * (1.0f - inhibition);
    return signal * effective_attention;
}

//=============================================================================
// Configuration
//=============================================================================

int dragonfly_thalamic_bridge_default_config(dragonfly_thalamic_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->initial_mode = THAL_ROUTE_DISCOVERY;

    /* LGN settings */
    config->lgn_channels = 32;
    config->lgn_attention_baseline = 0.5f;
    config->enable_lgn_burst_suppression = true;

    /* Pulvinar settings */
    config->pulvinar_gain = 1.2f;
    config->pulvinar_threshold = 0.2f;
    config->enable_pulvinar_feedback = false;

    /* Motor settings */
    config->motor_channels = 8;
    config->motor_gain = 1.0f;
    config->motor_urgency_boost = 1.5f;

    /* MD settings */
    config->decision_threshold = 0.3f;
    config->enable_decision_gating = true;

    /* Integration */
    config->update_rate_hz = 60.0f;
    config->sync_on_detection = true;
    config->enable_trn_gating = true;

    return 0;
}

int dragonfly_thalamic_bridge_validate_config(const dragonfly_thalamic_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_bridge_validate_config: config is NULL");
        return -1;
    }

    if (config->lgn_channels == 0 || config->lgn_channels > THAL_BRIDGE_MAX_VISUAL_CHANNELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_thalamic_bridge_validate_config: config->lgn_channels is zero");
        return -1;
    }
    if (config->lgn_attention_baseline < 0 || config->lgn_attention_baseline > 1.0f) {
        return -1;
    }
    if (config->pulvinar_gain < 0) {
        return -1;
    }
    if (config->pulvinar_threshold < 0 || config->pulvinar_threshold > 1.0f) {
        return -1;
    }
    if (config->motor_channels == 0 || config->motor_channels > THAL_BRIDGE_MAX_MOTOR_CHANNELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_thalamic_bridge_validate_config: config->motor_channels is zero");
        return -1;
    }
    if (config->motor_gain < 0) {
        return -1;
    }
    if (config->decision_threshold < 0 || config->decision_threshold > 1.0f) {
        return -1;
    }
    if (config->update_rate_hz <= 0) {
        return -1;
    }

    return 0;
}

//=============================================================================
// Lifecycle
//=============================================================================

dragonfly_thalamic_bridge_t* dragonfly_thalamic_bridge_create(
    dragonfly_system_t* dragonfly,
    void* thalamus,
    const dragonfly_thalamic_config_t* config
) {
    dragonfly_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(dragonfly_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_thalamic_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        if (dragonfly_thalamic_bridge_validate_config(config) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "dragonfly_thalamic_bridge_create: invalid configuration");
            nimcp_free(bridge);
            return NULL;
        }
        bridge->config = *config;
    } else {
        dragonfly_thalamic_bridge_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->dragonfly = dragonfly;
    bridge->thalamus = thalamus;

    /* Initialize state */
    bridge->current_mode = bridge->config.initial_mode;
    bridge->visual_attention = bridge->config.lgn_attention_baseline;
    bridge->motor_attention = 0.5f;
    bridge->attention_attention = 0.5f;
    bridge->decision_attention = 0.5f;

    bridge->visual_inhibition = 0.0f;
    bridge->motor_inhibition = 0.0f;
    bridge->attention_inhibition = 0.0f;
    bridge->decision_inhibition = 0.0f;

    /* Initialize bridge base infrastructure (mutex + metrics) */
    if (bridge_base_init(&bridge->base, 0, "dragonfly_thalamic") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->initialized = true;
    return bridge;
}

void dragonfly_thalamic_bridge_destroy(dragonfly_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_thalamic");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int dragonfly_thalamic_bridge_reset(dragonfly_thalamic_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_bridge_reset: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    bridge->current_mode = bridge->config.initial_mode;
    bridge->visual_attention = bridge->config.lgn_attention_baseline;
    bridge->motor_attention = 0.5f;
    bridge->attention_attention = 0.5f;
    bridge->decision_attention = 0.5f;

    bridge->visual_inhibition = 0.0f;
    bridge->motor_inhibition = 0.0f;
    bridge->attention_inhibition = 0.0f;
    bridge->decision_inhibition = 0.0f;

    memset(&bridge->last_visual, 0, sizeof(bridge->last_visual));
    memset(&bridge->last_motor, 0, sizeof(bridge->last_motor));
    memset(&bridge->last_attention, 0, sizeof(bridge->last_attention));
    memset(&bridge->last_decision, 0, sizeof(bridge->last_decision));
    memset(bridge->last_tsdn, 0, sizeof(bridge->last_tsdn));
    bridge->last_heading = 0;
    bridge->last_confidence = 0;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

//=============================================================================
// Signal Routing
//=============================================================================

int dragonfly_thalamic_relay_visual(
    dragonfly_thalamic_bridge_t* bridge,
    const thal_visual_target_t* target
) {
    if (!bridge || !bridge->initialized || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_relay_visual: required parameter is NULL (bridge, bridge->initialized, target)");
        return -1;
    }

    uint64_t start = get_time_us();

    /* Store for state tracking */
    bridge->last_visual = *target;

    /* Check if signal should be gated */
    float effective_attention = apply_attention_and_inhibition(
        1.0f, bridge->visual_attention, bridge->visual_inhibition);

    if (effective_attention < 0.1f) {
        bridge->stats.signals_gated++;
        return 0;  /* Signal gated by inhibition */
    }

    /* Route through thalamus if connected */
    if (bridge->thalamus) {
        /* Convert target to LGN-compatible signal */
        float lgn_signal[8];
        lgn_signal[0] = target->angular_position[0] * effective_attention;
        lgn_signal[1] = target->angular_position[1] * effective_attention;
        lgn_signal[2] = target->angular_velocity[0] * effective_attention;
        lgn_signal[3] = target->angular_velocity[1] * effective_attention;
        lgn_signal[4] = target->size * effective_attention;
        lgn_signal[5] = target->contrast * effective_attention;
        lgn_signal[6] = target->motion_energy * effective_attention;
        lgn_signal[7] = 0.0f;  /* Reserved */

        float output[8];
        thalamus_relay_visual(bridge->thalamus, lgn_signal, 8, output, 8);
    }

    bridge->stats.visual_signals_relayed++;
    bridge->stats.avg_lgn_attention = (bridge->stats.avg_lgn_attention * 0.95f) +
                                       (effective_attention * 0.05f);
    if (!isfinite(bridge->stats.avg_lgn_attention)) bridge->stats.avg_lgn_attention = effective_attention;
    bridge->stats.total_processing_time_us += get_time_us() - start;

    return 0;
}

int dragonfly_thalamic_relay_motor(
    dragonfly_thalamic_bridge_t* bridge,
    const thal_motor_command_t* command
) {
    if (!bridge || !bridge->initialized || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_relay_motor: required parameter is NULL (bridge, bridge->initialized, command)");
        return -1;
    }

    uint64_t start = get_time_us();

    bridge->last_motor = *command;

    float effective_attention = apply_attention_and_inhibition(
        1.0f, bridge->motor_attention, bridge->motor_inhibition);

    if (effective_attention < 0.1f) {
        bridge->stats.signals_gated++;
        return 0;
    }

    /* Apply urgency boost if configured */
    float gain = bridge->config.motor_gain;
    if (command->urgency > 0.7f) {
        gain *= bridge->config.motor_urgency_boost;
    }

    if (bridge->thalamus) {
        float motor_signal[4];
        motor_signal[0] = command->heading_adjustment[0] * gain * effective_attention;
        motor_signal[1] = command->heading_adjustment[1] * gain * effective_attention;
        motor_signal[2] = command->thrust * gain * effective_attention;
        motor_signal[3] = command->urgency;

        float output[4];
        thalamus_relay_motor(bridge->thalamus, motor_signal, 4, output, 4);
    }

    bridge->stats.motor_signals_relayed++;
    bridge->stats.total_processing_time_us += get_time_us() - start;

    return 0;
}

int dragonfly_thalamic_relay_attention(
    dragonfly_thalamic_bridge_t* bridge,
    const thal_attention_signal_t* attention
) {
    if (!bridge || !bridge->initialized || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_relay_attention: required parameter is NULL (bridge, bridge->initialized, attention)");
        return -1;
    }

    uint64_t start = get_time_us();

    bridge->last_attention = *attention;

    /* Check salience threshold */
    if (attention->salience < bridge->config.pulvinar_threshold) {
        return 0;  /* Below threshold */
    }

    float effective_attention = apply_attention_and_inhibition(
        1.0f, bridge->attention_attention, bridge->attention_inhibition);

    if (effective_attention < 0.1f) {
        bridge->stats.signals_gated++;
        return 0;
    }

    if (bridge->thalamus) {
        float pulvinar_signal[4];
        pulvinar_signal[0] = attention->salience * bridge->config.pulvinar_gain * effective_attention;
        pulvinar_signal[1] = attention->priority;
        pulvinar_signal[2] = attention->attention_width;
        pulvinar_signal[3] = attention->is_covert ? 0.0f : 1.0f;

        thalamus_pulvinar_attention(bridge->thalamus, pulvinar_signal, 4);
    }

    bridge->stats.attention_signals_relayed++;
    bridge->stats.avg_pulvinar_salience = (bridge->stats.avg_pulvinar_salience * 0.95f) +
                                           (attention->salience * 0.05f);
    if (!isfinite(bridge->stats.avg_pulvinar_salience)) bridge->stats.avg_pulvinar_salience = attention->salience;
    bridge->stats.total_processing_time_us += get_time_us() - start;

    return 0;
}

int dragonfly_thalamic_relay_decision(
    dragonfly_thalamic_bridge_t* bridge,
    const thal_decision_signal_t* decision
) {
    if (!bridge || !bridge->initialized || !decision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_relay_decision: required parameter is NULL (bridge, bridge->initialized, decision)");
        return -1;
    }

    uint64_t start = get_time_us();

    bridge->last_decision = *decision;

    /* Gate low-confidence decisions if configured */
    if (bridge->config.enable_decision_gating &&
        decision->confidence < bridge->config.decision_threshold) {
        return 0;
    }

    float effective_attention = apply_attention_and_inhibition(
        1.0f, bridge->decision_attention, bridge->decision_inhibition);

    if (effective_attention < 0.1f) {
        bridge->stats.signals_gated++;
        return 0;
    }

    if (bridge->thalamus) {
        float md_signal[4];
        md_signal[0] = (float)decision->action_code;
        md_signal[1] = decision->confidence * effective_attention;
        md_signal[2] = decision->expected_reward;
        md_signal[3] = decision->time_pressure;

        float output[4];
        thalamus_relay_executive(bridge->thalamus, md_signal, 4, output, 4);
    }

    bridge->stats.decision_signals_relayed++;
    bridge->stats.total_processing_time_us += get_time_us() - start;

    return 0;
}

int dragonfly_thalamic_relay_tsdn(
    dragonfly_thalamic_bridge_t* bridge,
    const float* tsdn_population,
    float heading_angle,
    float confidence
) {
    if (!bridge || !bridge->initialized || !tsdn_population) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_relay_tsdn: required parameter is NULL (bridge, bridge->initialized, tsdn_population)");
        return -1;
    }

    /* Store TSDN state */
    memcpy(bridge->last_tsdn, tsdn_population, sizeof(float) * THAL_BRIDGE_TSDN_CHANNELS);
    bridge->last_heading = heading_angle;
    bridge->last_confidence = confidence;

    /* Convert TSDN to visual target for LGN relay */
    thal_visual_target_t visual;
    memset(&visual, 0, sizeof(visual));
    visual.angular_position[0] = heading_angle;  /* Azimuth from heading */
    visual.angular_position[1] = 0.0f;           /* Elevation (2D tracking) */
    visual.motion_energy = confidence;
    visual.contrast = confidence;
    visual.size = 1.0f;  /* Unknown size */

    dragonfly_thalamic_relay_visual(bridge, &visual);

    /* Convert to attention signal for Pulvinar */
    if (confidence > bridge->config.pulvinar_threshold) {
        thal_attention_signal_t attention;
        memset(&attention, 0, sizeof(attention));
        attention.spatial_attention[0] = cosf(heading_angle);
        attention.spatial_attention[1] = sinf(heading_angle);
        attention.spatial_attention[2] = 0.0f;
        attention.attention_width = 0.5f;  /* Default width */
        attention.salience = confidence;
        attention.priority = confidence;
        attention.is_covert = true;  /* Dragonfly attention is covert */

        dragonfly_thalamic_relay_attention(bridge, &attention);
    }

    return 0;
}

//=============================================================================
// Mode Control
//=============================================================================

int dragonfly_thalamic_set_mode(
    dragonfly_thalamic_bridge_t* bridge,
    thal_routing_mode_t mode
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_set_mode: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    if (bridge->current_mode != mode) {
        bridge->current_mode = mode;
        bridge->stats.mode_switches++;

        /* Adjust attention levels based on mode */
        switch (mode) {
            case THAL_ROUTE_DISCOVERY:
                /* Burst mode - heightened sensitivity, lower specificity */
                bridge->visual_attention = 0.8f;
                bridge->motor_attention = 0.3f;
                break;

            case THAL_ROUTE_TRACKING:
                /* Tonic mode - faithful relay */
                bridge->visual_attention = bridge->config.lgn_attention_baseline;
                bridge->motor_attention = 0.6f;
                break;

            case THAL_ROUTE_INTERCEPT:
                /* High attention - all systems engaged */
                bridge->visual_attention = 1.0f;
                bridge->motor_attention = 1.0f;
                bridge->decision_attention = 1.0f;
                break;

            case THAL_ROUTE_SUPPRESSED:
                /* Suppress all signals */
                bridge->visual_inhibition = 0.9f;
                bridge->motor_inhibition = 0.9f;
                bridge->attention_inhibition = 0.9f;
                bridge->decision_inhibition = 0.9f;
                break;
        }

        /* Update thalamus firing mode if connected */
        if (bridge->thalamus) {
            thal_firing_mode_t thal_mode = (mode == THAL_ROUTE_DISCOVERY) ?
                THAL_MODE_BURST : THAL_MODE_TONIC;
            thalamus_set_mode(bridge->thalamus, THAL_NUCLEUS_LGN, thal_mode);
            thalamus_set_mode(bridge->thalamus, THAL_NUCLEUS_PULVINAR, thal_mode);
        }
    }

    return 0;
}

thal_routing_mode_t dragonfly_thalamic_get_mode(
    const dragonfly_thalamic_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) return THAL_ROUTE_SUPPRESSED;
    return bridge->current_mode;
}

int dragonfly_thalamic_set_attention(
    dragonfly_thalamic_bridge_t* bridge,
    thal_signal_type_t signal_type,
    float attention
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_set_attention: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    attention = nimcp_clampf(attention, 0.0f, 1.0f);

    switch (signal_type) {
        case THAL_SIGNAL_POSITION:
        case THAL_SIGNAL_VELOCITY:
            bridge->visual_attention = attention;
            break;
        case THAL_SIGNAL_SALIENCE:
            bridge->attention_attention = attention;
            break;
        case THAL_SIGNAL_MOTOR_CMD:
            bridge->motor_attention = attention;
            break;
        case THAL_SIGNAL_DECISION:
            bridge->decision_attention = attention;
            break;
    }

    return 0;
}

int dragonfly_thalamic_apply_inhibition(
    dragonfly_thalamic_bridge_t* bridge,
    thal_signal_type_t signal_type,
    float inhibition
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_apply_inhibition: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    inhibition = nimcp_clampf(inhibition, 0.0f, 1.0f);

    switch (signal_type) {
        case THAL_SIGNAL_POSITION:
        case THAL_SIGNAL_VELOCITY:
            bridge->visual_inhibition = inhibition;
            break;
        case THAL_SIGNAL_SALIENCE:
            bridge->attention_inhibition = inhibition;
            break;
        case THAL_SIGNAL_MOTOR_CMD:
            bridge->motor_inhibition = inhibition;
            break;
        case THAL_SIGNAL_DECISION:
            bridge->decision_inhibition = inhibition;
            break;
    }

    return 0;
}

//=============================================================================
// Integration
//=============================================================================

int dragonfly_thalamic_connect_dragonfly(
    dragonfly_thalamic_bridge_t* bridge,
    dragonfly_system_t* dragonfly
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_connect_dragonfly: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }
    bridge->dragonfly = dragonfly;
    return 0;
}

int dragonfly_thalamic_connect_thalamus(
    dragonfly_thalamic_bridge_t* bridge,
    void* thalamus
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_connect_thalamus: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }
    bridge->thalamus = thalamus;
    return 0;
}

bool dragonfly_thalamic_has_dragonfly(const dragonfly_thalamic_bridge_t* bridge) {
    return bridge && bridge->initialized && bridge->dragonfly != NULL;
}

bool dragonfly_thalamic_has_thalamus(const dragonfly_thalamic_bridge_t* bridge) {
    return bridge && bridge->initialized && bridge->thalamus != NULL;
}

//=============================================================================
// Update
//=============================================================================

int dragonfly_thalamic_update(dragonfly_thalamic_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_update: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* If dragonfly is connected, pull current state */
    if (bridge->dragonfly) {
        dragonfly_target_info_t target;
        if (dragonfly_get_primary_target(bridge->dragonfly, &target) == 0) {
            /* Relay target through TSDN pathway */
            float tsdn_population[16];
            memset(tsdn_population, 0, sizeof(tsdn_population));

            /* Create simple population from target direction */
            float heading = atan2f(target.velocity[1], target.velocity[0]);
            for (int i = 0; i < 16; i++) {
                float preferred = (float)i * (NIMCP_TWO_PI_F / 16.0f);
                float diff = heading - preferred;
                while (diff > 3.14159f) diff -= NIMCP_TWO_PI_F;
                while (diff < -3.14159f) diff += NIMCP_TWO_PI_F;
                tsdn_population[i] = expf(-diff * diff / 0.5f);
            }

            dragonfly_thalamic_relay_tsdn(bridge, tsdn_population, heading, target.confidence);
        }

        /* Update routing mode based on dragonfly mode */
        dragonfly_mode_t mode = dragonfly_get_mode(bridge->dragonfly);
        switch (mode) {
            case DRAGONFLY_MODE_SCANNING:
                dragonfly_thalamic_set_mode(bridge, THAL_ROUTE_DISCOVERY);
                break;
            case DRAGONFLY_MODE_TRACKING:
                dragonfly_thalamic_set_mode(bridge, THAL_ROUTE_TRACKING);
                break;
            case DRAGONFLY_MODE_PURSUING:
            case DRAGONFLY_MODE_INTERCEPTING:
                dragonfly_thalamic_set_mode(bridge, THAL_ROUTE_INTERCEPT);
                break;
            case DRAGONFLY_MODE_IDLE:
            default:
                dragonfly_thalamic_set_mode(bridge, THAL_ROUTE_SUPPRESSED);
                break;
        }
    }

    return 0;
}

int dragonfly_thalamic_step(
    dragonfly_thalamic_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_step: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }
    (void)dt_ms;

    return dragonfly_thalamic_update(bridge);
}

//=============================================================================
// Statistics
//=============================================================================

int dragonfly_thalamic_bridge_get_stats(
    const dragonfly_thalamic_bridge_t* bridge,
    thal_bridge_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_bridge_get_stats: required parameter is NULL (bridge, bridge->initialized, stats)");
        return -1;
    }

    *stats = bridge->stats;

    /* Compute average latency */
    uint64_t total_signals = stats->visual_signals_relayed +
                             stats->motor_signals_relayed +
                             stats->attention_signals_relayed +
                             stats->decision_signals_relayed;
    if (total_signals > 0) {
        stats->avg_relay_latency_us = (float)stats->total_processing_time_us / (float)total_signals;
    }

    return 0;
}

int dragonfly_thalamic_bridge_reset_stats(dragonfly_thalamic_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_thalamic_bridge_reset_stats: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

//=============================================================================
// Utility
//=============================================================================

const char* dragonfly_thalamic_mode_name(thal_routing_mode_t mode) {
    switch (mode) {
        case THAL_ROUTE_DISCOVERY:  return "discovery";
        case THAL_ROUTE_TRACKING:   return "tracking";
        case THAL_ROUTE_INTERCEPT:  return "intercept";
        case THAL_ROUTE_SUPPRESSED: return "suppressed";
        default:                    return "unknown";
    }
}

const char* dragonfly_thalamic_signal_name(thal_signal_type_t type) {
    switch (type) {
        case THAL_SIGNAL_POSITION:  return "position";
        case THAL_SIGNAL_VELOCITY:  return "velocity";
        case THAL_SIGNAL_SALIENCE:  return "salience";
        case THAL_SIGNAL_MOTOR_CMD: return "motor";
        case THAL_SIGNAL_DECISION:  return "decision";
        default:                    return "unknown";
    }
}
