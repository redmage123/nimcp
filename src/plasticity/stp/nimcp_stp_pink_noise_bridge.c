/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * stp_pink_noise_create has zero callers in production code. Wrapper
 * around pink_noise_create that is unused. Either wire a consumer or
 * delete before the next major version. Do not extend.
 */

/**
 * @file nimcp_stp_pink_noise_bridge.c
 * @brief Pink Noise - Short-Term Plasticity Integration Implementation
 * @version 1.0.0
 * @date 2025-12-21
 *
 * IMPLEMENTATION NOTES:
 * - Two separate pink noise generators: one for U, one for τ modulation
 * - State-dependent scaling uses sqrt(u×x) for smooth activity-dependent noise
 * - All modulation factors clamped to safe ranges to prevent numerical instability
 * - Statistics updated incrementally for O(1) overhead
 *
 * @author NIMCP Development Team
 */

#include "plasticity/stp/nimcp_stp_pink_noise_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "stp_pink_noise_bridge"

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stp_pink_noise_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(stp_pink_noise_bridge)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize noise generator configuration
 *
 * WHAT: Set up pink noise config for STP modulation
 * WHY:  Consistent initialization across U and τ generators
 * HOW:  Use biologically plausible defaults
 */
static void init_noise_config(pink_noise_config_t* config, float amplitude) {
    if (!config) return;

    config->alpha = STP_PINK_ALPHA_DEFAULT;
    config->amplitude = amplitude;
    config->min_frequency = 0.1f;   // 10s timescale
    config->max_frequency = 100.0f; // 10ms timescale
    config->sample_rate = STP_PINK_SAMPLE_RATE_DEFAULT;
    config->method = PINK_NOISE_VOSS;
    config->seed = 0; // Time-based
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default STP-Pink Noise configuration
 *
 * WHAT: Initialize config with biologically plausible defaults
 * WHY:  Standard starting point for STP noise modulation
 * HOW:  Set modulation amplitudes and enable all features
 */
int stp_pink_noise_default_config(stp_pink_noise_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "STP-pink noise config is NULL");

    memset(config, 0, sizeof(stp_pink_noise_config_t));

    // Release probability modulation
    config->u_noise_amplitude = STP_PINK_U_NOISE_AMPLITUDE;
    config->u_min_factor = STP_PINK_U_MIN_FACTOR;
    config->u_max_factor = STP_PINK_U_MAX_FACTOR;

    // Time constant modulation
    config->tau_d_noise_amplitude = STP_PINK_TAU_NOISE_AMPLITUDE;
    config->tau_f_noise_amplitude = STP_PINK_TAU_NOISE_AMPLITUDE;
    config->tau_min_factor = STP_PINK_TAU_MIN_FACTOR;
    config->tau_max_factor = STP_PINK_TAU_MAX_FACTOR;

    // Activity-dependent modulation
    config->enable_state_scaling = true;
    config->state_sensitivity = STP_PINK_STATE_SENSITIVITY;
    config->state_scaling_min = STP_PINK_STATE_SCALING_MIN;
    config->state_scaling_max = STP_PINK_STATE_SCALING_MAX;

    // Initialize noise configs
    init_noise_config(&config->u_noise_config, config->u_noise_amplitude);
    init_noise_config(&config->tau_noise_config, config->tau_d_noise_amplitude);

    // Enable all features
    config->enable_u_modulation = true;
    config->enable_tau_d_modulation = true;
    config->enable_tau_f_modulation = true;

    return 0;
}

/**
 * @brief Create STP-Pink Noise bridge
 *
 * WHAT: Allocate and initialize STP-pink noise integration bridge
 * WHY:  Enable stochastic modulation of STP parameters
 * HOW:  Allocate bridge, create noise generators, initialize state
 */
stp_pink_noise_bridge_t* stp_pink_noise_create(const stp_pink_noise_config_t* config) {
    // Use default config if NULL
    stp_pink_noise_config_t default_cfg;
    if (!config) {
        if (stp_pink_noise_default_config(&default_cfg) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stp_pink_noise_create: failed to initialize default config");
            return NULL;
        }
        config = &default_cfg;
    }

    // Allocate bridge
    stp_pink_noise_bridge_t* bridge = nimcp_malloc(sizeof(stp_pink_noise_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stp_pink_noise_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(stp_pink_noise_bridge_t));
    memcpy(&bridge->config, config, sizeof(stp_pink_noise_config_t));

    // Create noise generators
    bridge->u_noise_gen = pink_noise_create(&config->u_noise_config);
    if (!bridge->u_noise_gen) {
        NIMCP_LOGGING_ERROR("Failed to create U noise generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stp_pink_noise_create: failed to create U noise generator");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->tau_noise_gen = pink_noise_create(&config->tau_noise_config);
    if (!bridge->tau_noise_gen) {
        NIMCP_LOGGING_ERROR("Failed to create tau noise generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stp_pink_noise_create: failed to create tau noise generator");
        pink_noise_destroy(bridge->u_noise_gen);
        nimcp_free(bridge);
        return NULL;
    }

    // Initialize state
    bridge->noise_effects.effective_u_factor = 1.0f;
    bridge->noise_effects.effective_tau_d_factor = 1.0f;
    bridge->noise_effects.effective_tau_f_factor = 1.0f;
    bridge->noise_effects.current_state_scaling = 1.0f;

    bridge->stp_feedback.current_u = 0.5f;
    bridge->stp_feedback.current_x = 1.0f;
    bridge->stp_feedback.current_transmission = 0.5f;

    NIMCP_LOGGING_INFO("STP-Pink Noise bridge created");
    return bridge;
}

/**
 * @brief Destroy STP-Pink Noise bridge
 *
 * WHAT: Clean up all bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Destroy noise generators, free memory
 */
void stp_pink_noise_destroy(stp_pink_noise_bridge_t* bridge) {
    if (!bridge) return;

    // Disconnect bio-async if connected
    if (bridge->base.bio_async_enabled) {
        stp_pink_noise_disconnect_bio_async(bridge);
    }

    // Destroy noise generators
    if (bridge->u_noise_gen) {
        pink_noise_destroy(bridge->u_noise_gen);
    }
    if (bridge->tau_noise_gen) {
        pink_noise_destroy(bridge->tau_noise_gen);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("STP-Pink Noise bridge destroyed");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect STP state
 *
 * WHAT: Link bridge to STP state for modulation
 * WHY:  Enable parameter modulation
 * HOW:  Store pointer and extract base parameters
 */
int stp_pink_noise_connect_stp(
    stp_pink_noise_bridge_t* bridge,
    stp_state_t* stp_state
) {
    BRIDGE_BBB_VALIDATE(bridge, stp_state, sizeof(*stp_state));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_connect_stp: bridge is NULL");
        return -1;
    }
    if (!stp_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_connect_stp: stp_state is NULL");
        return -1;
    }

    bridge->stp_state = stp_state;

    // Extract base parameters
    bridge->state.base_u = stp_state->params.U;
    bridge->state.base_tau_d = stp_state->params.tau_D;
    bridge->state.base_tau_f = stp_state->params.tau_F;

    NIMCP_LOGGING_INFO("Connected to STP state");
    return 0;
}

/**
 * @brief Disconnect STP state
 *
 * WHAT: Unlink STP state
 * WHY:  Safe shutdown
 * HOW:  Clear pointer
 */
int stp_pink_noise_disconnect(stp_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_disconnect: bridge is NULL");
        return -1;
    }

    bridge->stp_state = NULL;
    NIMCP_LOGGING_INFO("Disconnected from STP state");
    return 0;
}

/* ============================================================================
 * Pink Noise → STP Direction
 * ============================================================================ */

/**
 * @brief Apply pink noise modulation to release probability
 *
 * WHAT: Generate noise sample and compute U modulation factor
 * WHY:  Stochastic vesicle release
 * HOW:  Sample noise, scale by state, clamp to valid range
 */
float stp_pink_noise_modulate_u(stp_pink_noise_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_u_modulation) {
        return 1.0f;
    }

    // Generate noise sample
    float noise_value = 0.0f;
    if (!pink_noise_generate_sample(bridge->u_noise_gen, &noise_value)) {
        NIMCP_LOGGING_WARN("Failed to generate U noise sample");
        return 1.0f;
    }

    // Store raw noise value
    bridge->noise_effects.u_noise_value = noise_value;

    // Apply state-dependent scaling if enabled
    float scaled_amplitude = bridge->config.u_noise_amplitude;
    if (bridge->config.enable_state_scaling) {
        scaled_amplitude *= bridge->noise_effects.current_state_scaling;
    }

    // Compute modulation factor: 1 + amplitude * noise
    float modulation = 1.0f + scaled_amplitude * noise_value;

    // Clamp to configured range
    modulation = nimcp_clampf(modulation, bridge->config.u_min_factor,
                      bridge->config.u_max_factor);

    bridge->noise_effects.effective_u_factor = modulation;
    return modulation;
}

/**
 * @brief Apply pink noise modulation to depression time constant
 *
 * WHAT: Generate noise sample and compute τ_D modulation factor
 * WHY:  Variable vesicle pool replenishment
 * HOW:  Sample noise, scale by state, clamp to valid range
 */
float stp_pink_noise_modulate_tau_d(stp_pink_noise_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_tau_d_modulation) {
        return 1.0f;
    }

    // Use tau noise generator
    float noise_value = 0.0f;
    if (!pink_noise_generate_sample(bridge->tau_noise_gen, &noise_value)) {
        NIMCP_LOGGING_WARN("Failed to generate tau_D noise sample");
        return 1.0f;
    }

    // Store raw noise value
    bridge->noise_effects.tau_d_noise_value = noise_value;

    // Apply state-dependent scaling if enabled
    float scaled_amplitude = bridge->config.tau_d_noise_amplitude;
    if (bridge->config.enable_state_scaling) {
        scaled_amplitude *= bridge->noise_effects.current_state_scaling;
    }

    // Compute modulation factor
    float modulation = 1.0f + scaled_amplitude * noise_value;

    // Clamp to configured range
    modulation = nimcp_clampf(modulation, bridge->config.tau_min_factor,
                      bridge->config.tau_max_factor);

    bridge->noise_effects.effective_tau_d_factor = modulation;
    return modulation;
}

/**
 * @brief Apply pink noise modulation to facilitation time constant
 *
 * WHAT: Generate noise sample and compute τ_F modulation factor
 * WHY:  Variable calcium buffering dynamics
 * HOW:  Sample noise, scale by state, clamp to valid range
 */
float stp_pink_noise_modulate_tau_f(stp_pink_noise_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_tau_f_modulation) {
        return 1.0f;
    }

    // Note: Could share noise with tau_D or use separate sample
    // Using tau_D noise value for correlation between τ_D and τ_F
    float noise_value = bridge->noise_effects.tau_d_noise_value;

    // Apply state-dependent scaling if enabled
    float scaled_amplitude = bridge->config.tau_f_noise_amplitude;
    if (bridge->config.enable_state_scaling) {
        scaled_amplitude *= bridge->noise_effects.current_state_scaling;
    }

    // Compute modulation factor
    float modulation = 1.0f + scaled_amplitude * noise_value;

    // Clamp to configured range
    modulation = nimcp_clampf(modulation, bridge->config.tau_min_factor,
                      bridge->config.tau_max_factor);

    bridge->noise_effects.effective_tau_f_factor = modulation;
    bridge->noise_effects.tau_f_noise_value = noise_value;

    return modulation;
}

/**
 * @brief Get effective release probability with noise
 *
 * WHAT: Compute noise-modulated U
 * WHY:  Convenience function for STP integration
 * HOW:  Multiply base U by modulation factor
 */
float stp_pink_noise_get_effective_u(
    const stp_pink_noise_bridge_t* bridge,
    float base_u
) {
    if (!bridge) return base_u;

    float effective_u = base_u * bridge->noise_effects.effective_u_factor;

    // Clamp to valid release probability range [0, 1]
    return nimcp_clampf(effective_u, 0.0f, 1.0f);
}

/**
 * @brief Get effective time constants with noise
 *
 * WHAT: Compute noise-modulated τ_D and τ_F
 * WHY:  Convenience function for STP integration
 * HOW:  Multiply base values by modulation factors
 */
int stp_pink_noise_get_effective_tau(
    const stp_pink_noise_bridge_t* bridge,
    float base_tau_d,
    float base_tau_f,
    float* tau_d_out,
    float* tau_f_out
) {
    BRIDGE_BBB_VALIDATE(bridge, tau_d_out, sizeof(*tau_d_out));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_get_effective_tau: bridge is NULL");
        return -1;
    }
    if (!tau_d_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_get_effective_tau: tau_d_out is NULL");
        return -1;
    }
    if (!tau_f_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_get_effective_tau: tau_f_out is NULL");
        return -1;
    }

    *tau_d_out = base_tau_d * bridge->noise_effects.effective_tau_d_factor;
    *tau_f_out = base_tau_f * bridge->noise_effects.effective_tau_f_factor;

    // Ensure positive time constants
    if (*tau_d_out < 0.1f) *tau_d_out = 0.1f;
    if (*tau_f_out < 0.1f) *tau_f_out = 0.1f;

    return 0;
}

/* ============================================================================
 * STP → Pink Noise Direction
 * ============================================================================ */

/**
 * @brief Update noise amplitude based on STP state
 *
 * WHAT: Scale noise amplitude by synaptic activity level
 * WHY:  High activity increases stochasticity (biological realism)
 * HOW:  amplitude_scale = sqrt(u × x) weighted by sensitivity
 */
float stp_pink_noise_scale_by_activity(
    stp_pink_noise_bridge_t* bridge,
    float u,
    float x
) {
    if (!bridge || !bridge->config.enable_state_scaling) {
        return 1.0f;
    }

    // Compute activity level: transmission = u × x
    float transmission = u * x;

    // Use sqrt for smooth scaling (avoid extreme values)
    float scaling = sqrtf(transmission);

    // Apply sensitivity
    scaling = 1.0f + (scaling - 1.0f) * bridge->config.state_sensitivity;

    // Clamp to configured range
    scaling = nimcp_clampf(scaling, bridge->config.state_scaling_min,
                   bridge->config.state_scaling_max);

    bridge->noise_effects.current_state_scaling = scaling;
    return scaling;
}

/**
 * @brief Report STP state to bridge
 *
 * WHAT: Update bridge with current STP state
 * WHY:  Enable activity-dependent noise modulation
 * HOW:  Store state variables and compute derived metrics
 */
int stp_pink_noise_report_stp_state(
    stp_pink_noise_bridge_t* bridge,
    float u,
    float x
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_report_stp_state: bridge is NULL");
        return -1;
    }

    // Update feedback state
    bridge->stp_feedback.current_u = u;
    bridge->stp_feedback.current_x = x;
    bridge->stp_feedback.current_transmission = u * x;

    // Compute activity-based noise amplitude scaling
    bridge->stp_feedback.activity_amplitude_scaling =
        stp_pink_noise_scale_by_activity(bridge, u, x);

    // Update bridge state
    bridge->state.current_u = u;
    bridge->state.current_x = x;

    return 0;
}

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update STP-Pink Noise bridge state
 *
 * WHAT: Generate new noise samples and update modulation factors
 * WHY:  Main integration loop for noise-based STP modulation
 * HOW:  Sample noise generators, compute modulation, update stats
 */
int stp_pink_noise_update(stp_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_update: bridge is NULL");
        return -1;
    }

    // Update STP state if connected
    if (bridge->stp_state) {
        stp_pink_noise_report_stp_state(
            bridge,
            bridge->stp_state->u,
            bridge->stp_state->x
        );
    }

    // Generate noise modulation factors
    float u_factor = stp_pink_noise_modulate_u(bridge);
    float tau_d_factor = stp_pink_noise_modulate_tau_d(bridge);
    float tau_f_factor = stp_pink_noise_modulate_tau_f(bridge);

    // Store modulation factors
    bridge->state.u_modulation = u_factor;
    bridge->state.tau_d_modulation = tau_d_factor;
    bridge->state.tau_f_modulation = tau_f_factor;
    bridge->state.state_scaling = bridge->noise_effects.current_state_scaling;

    // Update statistics
    bridge->stats.total_updates++;
    if (fabsf(u_factor - 1.0f) > 0.01f) {
        bridge->stats.u_modulated_events++;
    }
    if (fabsf(tau_d_factor - 1.0f) > 0.01f || fabsf(tau_f_factor - 1.0f) > 0.01f) {
        bridge->stats.tau_modulated_events++;
    }

    // Incremental averaging
    float n = (float)bridge->stats.total_updates;
    bridge->stats.avg_u_noise += (bridge->noise_effects.u_noise_value - bridge->stats.avg_u_noise) / n;
    bridge->stats.avg_tau_d_noise += (bridge->noise_effects.tau_d_noise_value - bridge->stats.avg_tau_d_noise) / n;
    bridge->stats.avg_tau_f_noise += (bridge->noise_effects.tau_f_noise_value - bridge->stats.avg_tau_f_noise) / n;
    bridge->stats.avg_u_modulation += (u_factor - bridge->stats.avg_u_modulation) / n;
    bridge->stats.avg_tau_d_modulation += (tau_d_factor - bridge->stats.avg_tau_d_modulation) / n;
    bridge->stats.avg_tau_f_modulation += (tau_f_factor - bridge->stats.avg_tau_f_modulation) / n;
    bridge->stats.avg_facilitation += (bridge->state.current_u - bridge->stats.avg_facilitation) / n;
    bridge->stats.avg_resources += (bridge->state.current_x - bridge->stats.avg_resources) / n;
    bridge->stats.avg_transmission += (bridge->stp_feedback.current_transmission - bridge->stats.avg_transmission) / n;
    bridge->stats.avg_state_scaling += (bridge->state.state_scaling - bridge->stats.avg_state_scaling) / n;

    bridge->state.modulation_events++;
    bridge->state.last_update_time++;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * WHAT: Copy current state to output
 * WHY:  Monitoring and debugging
 * HOW:  Memcpy state struct
 */
int stp_pink_noise_get_state(
    const stp_pink_noise_bridge_t* bridge,
    stp_pink_noise_state_t* state
) {
    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_get_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_get_state: state is NULL");
        return -1;
    }

    memcpy(state, &bridge->state, sizeof(stp_pink_noise_state_t));
    return 0;
}

/**
 * @brief Get bridge statistics
 *
 * WHAT: Copy statistics to output
 * WHY:  Performance monitoring
 * HOW:  Memcpy stats struct
 */
int stp_pink_noise_get_stats(
    const stp_pink_noise_bridge_t* bridge,
    stp_pink_noise_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_get_stats: stats is NULL");
        return -1;
    }

    memcpy(stats, &bridge->stats, sizeof(stp_pink_noise_stats_t));
    return 0;
}

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear all accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zero stats struct
 */
int stp_pink_noise_reset_stats(stp_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_reset_stats: bridge is NULL");
        return -1;
    }

    memset(&bridge->stats, 0, sizeof(stp_pink_noise_stats_t));
    NIMCP_LOGGING_INFO("Statistics reset");
    return 0;
}

/* ============================================================================
 * Feature Control
 * ============================================================================ */

/**
 * @brief Enable/disable pink noise modulation
 *
 * WHAT: Toggle noise modulation features
 * WHY:  Dynamic control for testing
 * HOW:  Set enable flags
 */
int stp_pink_noise_enable(stp_pink_noise_bridge_t* bridge, bool enable) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_enable: bridge is NULL");
        return -1;
    }

    bridge->config.enable_u_modulation = enable;
    bridge->config.enable_tau_d_modulation = enable;
    bridge->config.enable_tau_f_modulation = enable;

    NIMCP_LOGGING_INFO("Pink noise modulation %s", enable ? "enabled" : "disabled");
    return 0;
}

/**
 * @brief Check if pink noise modulation is enabled
 *
 * WHAT: Query modulation state
 * WHY:  Conditional behavior
 * HOW:  Check any enable flag
 */
bool stp_pink_noise_is_enabled(const stp_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    return bridge->config.enable_u_modulation ||
           bridge->config.enable_tau_d_modulation ||
           bridge->config.enable_tau_f_modulation;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register module with bio-async router
 * WHY:  Enable distributed noise signaling
 * HOW:  Register module, store context
 */
int stp_pink_noise_connect_bio_async(stp_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_pink_noise_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0; // Already connected
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_STP_PINK_NOISE_BRIDGE,
        .module_name = "stp_pink_noise_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stp_pink_noise_connect_bio_async: validation failed");
    return -1;
}

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from router
 * WHY:  Clean shutdown
 * HOW:  Unregister module, clear context
 */
int stp_pink_noise_disconnect_bio_async(stp_pink_noise_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection state
 * WHY:  Conditional message sending
 * HOW:  Check enable flag
 */
bool stp_pink_noise_is_bio_async_connected(
    const stp_pink_noise_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
