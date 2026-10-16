/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * stdp_pink_noise_create has zero callers in production code. Wrapper
 * around pink_noise_create that is unused. Either wire a consumer or
 * delete before the next major version. Do not extend.
 */

#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_stdp_pink_noise_bridge.c - Pink Noise Bridge Implementation
//=============================================================================

#include "plasticity/stdp/nimcp_stdp_pink_noise_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stdp_pink_noise_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(stdp_pink_noise_bridge)


//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Create pink noise bridge for STDP optimizer
 * WHY:  Enable quantum-inspired stochastic learning rate modulation
 * HOW:  Allocate state, initialize pink quantum bridge with config
 */
stdp_pink_noise_bridge_t* stdp_pink_noise_create(
    const stdp_pink_noise_config_t* config
) {
    /* Allocate bridge structure */
    stdp_pink_noise_bridge_t* bridge =
        (stdp_pink_noise_bridge_t*)nimcp_calloc(1, sizeof(stdp_pink_noise_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "STDP pink noise bridge allocation failed");

    /* Set configuration */
    bridge->config = config ? *config : stdp_pink_noise_default_config();

    /* Create quantum pink noise generator */
    pink_quantum_config_t pink_config = pink_quantum_default_config();
    pink_config.target_alpha = bridge->config.noise_alpha;
    pink_config.amplitude = bridge->config.noise_amplitude;
    pink_config.sample_rate = bridge->config.noise_sample_rate;
    pink_config.method = bridge->config.quantum_method;
    pink_config.seed = bridge->config.seed;

    bridge->pink_bridge = pink_quantum_create(&pink_config);
    if (!bridge->pink_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_pink_noise_create: failed to create quantum pink noise generator");
        NIMCP_LOGGING_ERROR("Failed to create quantum pink noise generator");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize state */
    bridge->is_enabled = bridge->config.enabled;
    bridge->noise_connected = true;
    bridge->optimizer_connected = false;

    /* Initialize noisy parameters to safe defaults */
    bridge->noisy_lr = 0.01f;
    bridge->noisy_a_plus = 0.005f;
    bridge->noisy_a_minus = 0.005f;
    bridge->noisy_tau_plus = 20.0f;
    bridge->noisy_tau_minus = 20.0f;

    NIMCP_LOGGING_INFO("Created STDP pink noise bridge (alpha=%.2f, amp=%.3f)",
                      bridge->config.noise_alpha,
                      bridge->config.noise_amplitude);

    return bridge;
}

/**
 * WHAT: Destroy pink noise bridge
 * WHY:  Free all allocated resources
 * HOW:  Destroy pink bridge, free state
 */
void stdp_pink_noise_destroy(stdp_pink_noise_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy pink quantum bridge */
    if (bridge->pink_bridge) {
        pink_quantum_destroy(bridge->pink_bridge);
    }

    nimcp_free(bridge);
}

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * WHAT: Connect STDP optimizer to bridge
 * WHY:  Bridge needs optimizer reference to read/modulate parameters
 * HOW:  Store optimizer handle, validate, set connection flag
 */
int stdp_pink_noise_connect_optimizer(
    stdp_pink_noise_bridge_t* bridge,
    qstdp_optimizer_t optimizer
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP pink noise bridge is NULL");
    NIMCP_API_CHECK_NULL(optimizer, -1, "QSTDP optimizer is NULL");

    bridge->stdp_optimizer = optimizer;
    bridge->optimizer_connected = true;

    NIMCP_LOGGING_INFO("Connected STDP optimizer to pink noise bridge");
    return 0;
}

/**
 * WHAT: Disconnect STDP optimizer
 * WHY:  Remove optimizer reference
 * HOW:  Clear handle, reset connection flag
 */
int stdp_pink_noise_disconnect_optimizer(stdp_pink_noise_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP pink noise bridge is NULL");

    bridge->stdp_optimizer = NULL;
    bridge->optimizer_connected = false;

    NIMCP_LOGGING_INFO("Disconnected STDP optimizer from pink noise bridge");
    return 0;
}

/**
 * WHAT: Check if optimizer is connected
 * WHY:  Guard against operations requiring optimizer
 * HOW:  Return connection flag
 */
bool stdp_pink_noise_has_optimizer(const stdp_pink_noise_bridge_t* bridge) {
    return bridge && bridge->optimizer_connected && bridge->stdp_optimizer != NULL;
}

//=============================================================================
// Noise Generation and Application
//=============================================================================

/**
 * WHAT: Update noise samples from quantum pink noise generator
 * WHY:  Refresh noise for current optimization step
 * HOW:  Generate samples for each parameter based on targets
 */
int stdp_pink_noise_update_samples(stdp_pink_noise_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP pink noise bridge is NULL");

    if (!bridge->is_enabled) {
        /* Zero out noise when disabled */
        bridge->lr_noise = 0.0f;
        bridge->a_plus_noise = 0.0f;
        bridge->a_minus_noise = 0.0f;
        bridge->tau_plus_noise = 0.0f;
        bridge->tau_minus_noise = 0.0f;
        return 0;
    }

    if (!bridge->pink_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_pink_noise_update_samples: pink quantum bridge not initialized");
        NIMCP_LOGGING_ERROR("Pink quantum bridge not initialized");
        return -1;
    }

    /* Generate noise for targeted parameters */
    if (bridge->config.noise_targets & STDP_NOISE_TARGET_LR) {
        if (pink_quantum_generate_sample(bridge->pink_bridge, &bridge->lr_noise) != 0) {
            NIMCP_LOGGING_WARN("Failed to generate LR noise sample");
            bridge->lr_noise = 0.0f;
        }
        bridge->lr_noise *= bridge->config.lr_noise_scale;
    } else {
        bridge->lr_noise = 0.0f;
    }

    if (bridge->config.noise_targets & STDP_NOISE_TARGET_AMPLITUDES) {
        float a_noise;
        if (pink_quantum_generate_sample(bridge->pink_bridge, &a_noise) != 0) {
            NIMCP_LOGGING_WARN("Failed to generate amplitude noise sample");
            a_noise = 0.0f;
        }
        bridge->a_plus_noise = a_noise * bridge->config.amplitude_noise_scale;
        bridge->a_minus_noise = a_noise * bridge->config.amplitude_noise_scale;
    } else {
        bridge->a_plus_noise = 0.0f;
        bridge->a_minus_noise = 0.0f;
    }

    if (bridge->config.noise_targets & STDP_NOISE_TARGET_TAUS) {
        float tau_noise;
        if (pink_quantum_generate_sample(bridge->pink_bridge, &tau_noise) != 0) {
            NIMCP_LOGGING_WARN("Failed to generate tau noise sample");
            tau_noise = 0.0f;
        }
        bridge->tau_plus_noise = tau_noise * bridge->config.tau_noise_scale;
        bridge->tau_minus_noise = tau_noise * bridge->config.tau_noise_scale;
    } else {
        bridge->tau_plus_noise = 0.0f;
        bridge->tau_minus_noise = 0.0f;
    }

    /* Update statistics */
    bridge->samples_generated++;
    float total_noise = fabsf(bridge->lr_noise) + fabsf(bridge->a_plus_noise) +
                       fabsf(bridge->a_minus_noise) + fabsf(bridge->tau_plus_noise) +
                       fabsf(bridge->tau_minus_noise);
    float avg_noise = total_noise / 5.0f;

    bridge->avg_noise_amplitude =
        (bridge->avg_noise_amplitude * (bridge->samples_generated - 1) + avg_noise) /
        bridge->samples_generated;

    if (avg_noise > bridge->max_noise_amplitude) {
        bridge->max_noise_amplitude = avg_noise;
    }

    return 0;
}

/**
 * WHAT: Apply noise to STDP parameters
 * WHY:  Inject quantum-inspired stochasticity for robust learning
 * HOW:  Read base params, apply noise based on mode, clamp to bounds
 */
int stdp_pink_noise_apply_modulation(stdp_pink_noise_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP pink noise bridge is NULL");

    if (!stdp_pink_noise_has_optimizer(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_pink_noise_apply_modulation: no optimizer connected");
        NIMCP_LOGGING_ERROR("No optimizer connected for noise modulation");
        return -1;
    }

    /* Get base parameters from optimizer */
    float base_lr, base_a_plus, base_a_minus, base_tau_plus, base_tau_minus;
    if (qstdp_optimizer_get_params(bridge->stdp_optimizer,
                                    &base_lr, &base_a_plus, &base_a_minus,
                                    &base_tau_plus, &base_tau_minus) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_pink_noise_apply_modulation: failed to get base parameters");
        NIMCP_LOGGING_ERROR("Failed to get base parameters from optimizer");
        return -1;
    }

    /* Update noise samples */
    if (stdp_pink_noise_update_samples(bridge) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_pink_noise_apply_modulation: failed to update noise samples");
        NIMCP_LOGGING_ERROR("Failed to update noise samples");
        return -1;
    }

    /* Get current quantum temperature for temperature coupling */
    float temperature_scale = 1.0f;
    if (bridge->config.couple_to_temperature) {
        qstdp_optimizer_stats_t opt_stats;
        if (qstdp_optimizer_get_stats(bridge->stdp_optimizer, &opt_stats) == 0) {
            float current_temp = opt_stats.current_temperature;

            /* Disable noise below threshold temperature */
            if (current_temp < bridge->config.min_temperature_threshold) {
                temperature_scale = 0.0f;
            } else {
                /* Scale noise proportionally to temperature */
                temperature_scale = current_temp;
            }
        }
    }

    /* Apply noise based on mode */
    switch (bridge->config.noise_mode) {
        case STDP_NOISE_ADDITIVE:
            bridge->noisy_lr = base_lr + bridge->lr_noise * temperature_scale;
            bridge->noisy_a_plus = base_a_plus + bridge->a_plus_noise * temperature_scale;
            bridge->noisy_a_minus = base_a_minus + bridge->a_minus_noise * temperature_scale;
            bridge->noisy_tau_plus = base_tau_plus + bridge->tau_plus_noise * temperature_scale;
            bridge->noisy_tau_minus = base_tau_minus + bridge->tau_minus_noise * temperature_scale;
            break;

        case STDP_NOISE_MULTIPLICATIVE:
            bridge->noisy_lr = base_lr * (1.0f + bridge->lr_noise * temperature_scale);
            bridge->noisy_a_plus = base_a_plus * (1.0f + bridge->a_plus_noise * temperature_scale);
            bridge->noisy_a_minus = base_a_minus * (1.0f + bridge->a_minus_noise * temperature_scale);
            bridge->noisy_tau_plus = base_tau_plus * (1.0f + bridge->tau_plus_noise * temperature_scale);
            bridge->noisy_tau_minus = base_tau_minus * (1.0f + bridge->tau_minus_noise * temperature_scale);
            break;

        case STDP_NOISE_TEMPERATURE_SCALED:
            /* Noise amplitude proportional to temperature */
            bridge->noisy_lr = base_lr + bridge->lr_noise * temperature_scale * temperature_scale;
            bridge->noisy_a_plus = base_a_plus + bridge->a_plus_noise * temperature_scale * temperature_scale;
            bridge->noisy_a_minus = base_a_minus + bridge->a_minus_noise * temperature_scale * temperature_scale;
            bridge->noisy_tau_plus = base_tau_plus + bridge->tau_plus_noise * temperature_scale * temperature_scale;
            bridge->noisy_tau_minus = base_tau_minus + bridge->tau_minus_noise * temperature_scale * temperature_scale;
            break;

        case STDP_NOISE_ADAPTIVE:
            /* Switch between additive (high temp) and multiplicative (low temp) */
            if (temperature_scale > 0.5f) {
                /* High temp: additive for exploration */
                bridge->noisy_lr = base_lr + bridge->lr_noise * temperature_scale;
                bridge->noisy_a_plus = base_a_plus + bridge->a_plus_noise * temperature_scale;
                bridge->noisy_a_minus = base_a_minus + bridge->a_minus_noise * temperature_scale;
                bridge->noisy_tau_plus = base_tau_plus + bridge->tau_plus_noise * temperature_scale;
                bridge->noisy_tau_minus = base_tau_minus + bridge->tau_minus_noise * temperature_scale;
            } else {
                /* Low temp: multiplicative for fine-tuning */
                bridge->noisy_lr = base_lr * (1.0f + bridge->lr_noise * temperature_scale);
                bridge->noisy_a_plus = base_a_plus * (1.0f + bridge->a_plus_noise * temperature_scale);
                bridge->noisy_a_minus = base_a_minus * (1.0f + bridge->a_minus_noise * temperature_scale);
                bridge->noisy_tau_plus = base_tau_plus * (1.0f + bridge->tau_plus_noise * temperature_scale);
                bridge->noisy_tau_minus = base_tau_minus * (1.0f + bridge->tau_minus_noise * temperature_scale);
            }
            break;
    }

    /* P0 fix: Validate NaN/Inf BEFORE clamping to safety bounds
     * WHY:  NaN/Inf propagates through fmaxf/fminf and corrupts the result
     * HOW:  Reset to safe default values if invalid, then clamp
     */
    if (isnan(bridge->noisy_lr) || isinf(bridge->noisy_lr)) {
        bridge->noisy_lr = base_lr;  /* Reset to base value */
    }
    if (isnan(bridge->noisy_a_plus) || isinf(bridge->noisy_a_plus)) {
        bridge->noisy_a_plus = base_a_plus;
    }
    if (isnan(bridge->noisy_a_minus) || isinf(bridge->noisy_a_minus)) {
        bridge->noisy_a_minus = base_a_minus;
    }
    if (isnan(bridge->noisy_tau_plus) || isinf(bridge->noisy_tau_plus)) {
        bridge->noisy_tau_plus = base_tau_plus;
    }
    if (isnan(bridge->noisy_tau_minus) || isinf(bridge->noisy_tau_minus)) {
        bridge->noisy_tau_minus = base_tau_minus;
    }

    /* Clamp to safety bounds (now safe since NaN/Inf removed above) */
    bridge->noisy_lr = fmaxf(bridge->config.lr_min,
                            fminf(bridge->config.lr_max, bridge->noisy_lr));
    bridge->noisy_a_plus = fmaxf(bridge->config.a_min,
                                fminf(bridge->config.a_max, bridge->noisy_a_plus));
    bridge->noisy_a_minus = fmaxf(bridge->config.a_min,
                                 fminf(bridge->config.a_max, bridge->noisy_a_minus));
    bridge->noisy_tau_plus = fmaxf(bridge->config.tau_min,
                                  fminf(bridge->config.tau_max, bridge->noisy_tau_plus));
    bridge->noisy_tau_minus = fmaxf(bridge->config.tau_min,
                                   fminf(bridge->config.tau_max, bridge->noisy_tau_minus));

    /* Update statistics */
    bridge->parameters_modulated++;

    return 0;
}

/**
 * WHAT: Get modulated learning rate
 * WHY:  Access current noisy LR for STDP application
 * HOW:  Return cached value from last apply_modulation
 */
float stdp_pink_noise_get_noisy_lr(const stdp_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pink_noise_get_noisy_lr: bridge is NULL");
        return 0.01f;
    }
    return bridge->noisy_lr;
}

/**
 * WHAT: Get all modulated parameters
 * WHY:  Batch access to noisy STDP parameters
 * HOW:  Copy cached values to output pointers
 */
int stdp_pink_noise_get_noisy_params(
    const stdp_pink_noise_bridge_t* bridge,
    float* lr,
    float* a_plus,
    float* a_minus,
    float* tau_plus,
    float* tau_minus
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP pink noise bridge is NULL");
    BRIDGE_BBB_VALIDATE(bridge, lr, sizeof(*lr));

    if (lr) *lr = bridge->noisy_lr;
    if (a_plus) *a_plus = bridge->noisy_a_plus;
    if (a_minus) *a_minus = bridge->noisy_a_minus;
    if (tau_plus) *tau_plus = bridge->noisy_tau_plus;
    if (tau_minus) *tau_minus = bridge->noisy_tau_minus;

    return 0;
}

//=============================================================================
// Control Functions
//=============================================================================

/**
 * WHAT: Enable/disable pink noise bridge
 * WHY:  Allow runtime control of noise injection
 * HOW:  Set enabled flag, zero noise when disabled
 */
int stdp_pink_noise_set_enabled(
    stdp_pink_noise_bridge_t* bridge,
    bool enabled
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP pink noise bridge is NULL");

    bridge->is_enabled = enabled;
    bridge->config.enabled = enabled;

    if (!enabled) {
        /* Zero out noise when disabled */
        bridge->lr_noise = 0.0f;
        bridge->a_plus_noise = 0.0f;
        bridge->a_minus_noise = 0.0f;
        bridge->tau_plus_noise = 0.0f;
        bridge->tau_minus_noise = 0.0f;
    }

    NIMCP_LOGGING_INFO("Pink noise bridge %s", enabled ? "enabled" : "disabled");
    return 0;
}

/**
 * WHAT: Check if bridge is enabled
 * WHY:  Guard against disabled operations
 * HOW:  Return enabled flag
 */
bool stdp_pink_noise_is_enabled(const stdp_pink_noise_bridge_t* bridge) {
    return bridge && bridge->is_enabled;
}

/**
 * WHAT: Set noise application mode
 * WHY:  Switch between additive/multiplicative/etc.
 * HOW:  Update config mode
 */
int stdp_pink_noise_set_mode(
    stdp_pink_noise_bridge_t* bridge,
    stdp_noise_mode_t mode
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP pink noise bridge is NULL");

    bridge->config.noise_mode = mode;
    NIMCP_LOGGING_INFO("Set noise mode to %s", stdp_noise_mode_name(mode));
    return 0;
}

/**
 * WHAT: Set which parameters receive noise
 * WHY:  Control scope of noise modulation
 * HOW:  Update target bitfield
 */
int stdp_pink_noise_set_targets(
    stdp_pink_noise_bridge_t* bridge,
    stdp_noise_target_t targets
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP pink noise bridge is NULL");

    bridge->config.noise_targets = targets;
    NIMCP_LOGGING_INFO("Set noise targets to 0x%02X", targets);
    return 0;
}

/**
 * WHAT: Set quantum noise method
 * WHY:  Switch between annealing/ternary/walk/hybrid
 * HOW:  Update pink bridge method
 */
int stdp_pink_noise_set_quantum_method(
    stdp_pink_noise_bridge_t* bridge,
    pink_quantum_method_t method
) {
    if (!bridge || !bridge->pink_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "stdp_pink_noise_set_quantum_method: invalid parameter");
        return -1;
    }

    bridge->config.quantum_method = method;
    return pink_quantum_set_method(bridge->pink_bridge, method);
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * WHAT: Get bridge statistics
 * WHY:  Monitor noise impact and quantum performance
 * HOW:  Aggregate bridge stats and pink quantum stats
 */
int stdp_pink_noise_get_stats(
    const stdp_pink_noise_bridge_t* bridge,
    stdp_pink_noise_stats_t* stats
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP pink noise bridge is NULL");
    NIMCP_API_CHECK_NULL(stats, -1, "Stats output pointer is NULL");

    stats->samples_generated = bridge->samples_generated;
    stats->parameters_modulated = bridge->parameters_modulated;
    stats->avg_noise_amplitude = bridge->avg_noise_amplitude;
    stats->max_noise_amplitude = bridge->max_noise_amplitude;
    stats->current_noisy_lr = bridge->noisy_lr;
    stats->noise_impact_on_energy = bridge->noise_impact_on_energy;

    /* Get quantum bridge stats */
    if (bridge->pink_bridge) {
        pink_quantum_get_stats(bridge->pink_bridge, &stats->quantum_stats);
    } else {
        memset(&stats->quantum_stats, 0, sizeof(stats->quantum_stats));
    }

    return 0;
}

/**
 * WHAT: Reset statistics counters
 * WHY:  Start fresh tracking
 * HOW:  Zero all stat fields
 */
int stdp_pink_noise_reset_stats(stdp_pink_noise_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP pink noise bridge is NULL");

    bridge->samples_generated = 0;
    bridge->parameters_modulated = 0;
    bridge->avg_noise_amplitude = 0.0f;
    bridge->max_noise_amplitude = 0.0f;
    bridge->noise_impact_on_energy = 0.0f;

    /* Reset pink quantum stats */
    if (bridge->pink_bridge) {
        pink_quantum_reset_stats(bridge->pink_bridge);
    }

    NIMCP_LOGGING_INFO("Reset pink noise bridge statistics");
    return 0;
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Start fresh noise sequence
 * HOW:  Reset pink quantum bridge, clear cached params, reseed
 */
int stdp_pink_noise_reset(
    stdp_pink_noise_bridge_t* bridge,
    uint32_t new_seed
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP pink noise bridge is NULL");

    /* Reset pink quantum bridge */
    if (bridge->pink_bridge) {
        uint32_t seed = new_seed ? new_seed : bridge->config.seed;
        if (pink_quantum_reset(bridge->pink_bridge, seed) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_pink_noise_reset: failed to reset pink quantum bridge");
            NIMCP_LOGGING_ERROR("Failed to reset pink quantum bridge");
            return -1;
        }
    }

    /* Clear cached noise samples */
    bridge->lr_noise = 0.0f;
    bridge->a_plus_noise = 0.0f;
    bridge->a_minus_noise = 0.0f;
    bridge->tau_plus_noise = 0.0f;
    bridge->tau_minus_noise = 0.0f;

    /* Reset noisy parameters to defaults */
    bridge->noisy_lr = 0.01f;
    bridge->noisy_a_plus = 0.005f;
    bridge->noisy_a_minus = 0.005f;
    bridge->noisy_tau_plus = 20.0f;
    bridge->noisy_tau_minus = 20.0f;

    /* Reset statistics */
    stdp_pink_noise_reset_stats(bridge);

    NIMCP_LOGGING_INFO("Reset pink noise bridge");
    return 0;
}

/**
 * WHAT: Get noise mode name as string
 * WHY:  Logging and debugging
 * HOW:  Enum-to-string mapping
 */
const char* stdp_noise_mode_name(stdp_noise_mode_t mode) {
    switch (mode) {
        case STDP_NOISE_ADDITIVE:           return "Additive";
        case STDP_NOISE_MULTIPLICATIVE:     return "Multiplicative";
        case STDP_NOISE_TEMPERATURE_SCALED: return "TemperatureScaled";
        case STDP_NOISE_ADAPTIVE:           return "Adaptive";
        default:                            return "Unknown";
    }
}
