#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_pink_noise_immune_bridge.c - Pink Noise Immune System Integration
//=============================================================================
/**
 * WHAT: Bidirectional integration between immune system and pink noise
 * WHY:  Inflammation affects neural noise; noise statistics inform immune
 * HOW:  Cytokine levels modulate amplitude/spectrum; variance feeds back
 */

#include "plasticity/noise/nimcp_pink_noise_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pink_noise_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(pink_immune_bridge)


//=============================================================================
// Default Configuration
//=============================================================================

pink_immune_config_t pink_immune_bridge_default_config(void) {
    pink_immune_config_t config = {0};

    // IL-1β: Pro-inflammatory, increases noise amplitude
    config.cytokine_effects[PINK_CYTOKINE_IL1].amplitude_factor = 1.3f;
    config.cytokine_effects[PINK_CYTOKINE_IL1].alpha_shift = -0.1f;
    config.cytokine_effects[PINK_CYTOKINE_IL1].threshold = 0.2f;

    // IL-6: Pro-inflammatory, moderate effect
    config.cytokine_effects[PINK_CYTOKINE_IL6].amplitude_factor = 1.15f;
    config.cytokine_effects[PINK_CYTOKINE_IL6].alpha_shift = -0.05f;
    config.cytokine_effects[PINK_CYTOKINE_IL6].threshold = 0.3f;

    // IL-10: Anti-inflammatory, restores normal
    config.cytokine_effects[PINK_CYTOKINE_IL10].amplitude_factor = 0.9f;
    config.cytokine_effects[PINK_CYTOKINE_IL10].alpha_shift = 0.05f;
    config.cytokine_effects[PINK_CYTOKINE_IL10].threshold = 0.2f;

    // TNF-α: Pro-inflammatory, whitens spectrum
    config.cytokine_effects[PINK_CYTOKINE_TNF].amplitude_factor = 1.4f;
    config.cytokine_effects[PINK_CYTOKINE_TNF].alpha_shift = -0.2f;
    config.cytokine_effects[PINK_CYTOKINE_TNF].threshold = 0.25f;

    // IFN-γ: Complex effect on neural activity
    config.cytokine_effects[PINK_CYTOKINE_IFN_GAMMA].amplitude_factor = 1.1f;
    config.cytokine_effects[PINK_CYTOKINE_IFN_GAMMA].alpha_shift = 0.0f;
    config.cytokine_effects[PINK_CYTOKINE_IFN_GAMMA].threshold = 0.3f;

    // Inflammation level effects
    // Amplitude scaling per level (multiplicative)
    config.inflammation_amplitude_scale[PINK_INFLAMMATION_NONE] = 1.0f;
    config.inflammation_amplitude_scale[PINK_INFLAMMATION_LOCAL] = 1.1f;
    config.inflammation_amplitude_scale[PINK_INFLAMMATION_REGIONAL] = 1.25f;
    config.inflammation_amplitude_scale[PINK_INFLAMMATION_SYSTEMIC] = 1.5f;
    config.inflammation_amplitude_scale[PINK_INFLAMMATION_STORM] = 2.0f;

    // Alpha shift per level (additive, negative = whiter)
    config.inflammation_alpha_shift[PINK_INFLAMMATION_NONE] = 0.0f;
    config.inflammation_alpha_shift[PINK_INFLAMMATION_LOCAL] = -0.05f;
    config.inflammation_alpha_shift[PINK_INFLAMMATION_REGIONAL] = -0.15f;
    config.inflammation_alpha_shift[PINK_INFLAMMATION_SYSTEMIC] = -0.3f;
    config.inflammation_alpha_shift[PINK_INFLAMMATION_STORM] = -0.5f;

    // Feedback thresholds
    config.abnormal_alpha_threshold = 0.3f;    // |α - 1.0| > 0.3 is abnormal
    config.variance_warning_threshold = 0.5f;  // Variance > 0.5 is warning
    config.feedback_gain = 0.1f;

    // Base parameters
    config.base_amplitude = 0.05f;
    config.base_alpha = 1.0f;

    config.enable_immune_modulation = true;
    config.enable_noise_feedback = true;

    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pink_immune_bridge_t* pink_immune_bridge_create(
    const pink_immune_config_t* config
) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    pink_immune_bridge_t* bridge = nimcp_calloc(1, sizeof(pink_immune_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    memcpy(&bridge->config, config, sizeof(pink_immune_config_t));

    // Initialize effects to neutral
    bridge->effects.amplitude_modifier = 1.0f;
    bridge->effects.alpha_modifier = 0.0f;
    bridge->effects.effective_amplitude = config->base_amplitude;
    bridge->effects.effective_alpha = config->base_alpha;

    // Initialize immune state
    bridge->immune_state.inflammation = PINK_INFLAMMATION_NONE;

    NIMCP_LOGGING_INFO("Created pink noise immune bridge");
    return bridge;
}

void pink_immune_bridge_destroy(pink_immune_bridge_t* bridge) {
    if (!bridge) return;
    nimcp_free(bridge);
}

//=============================================================================
// Connection Functions
//=============================================================================

int pink_immune_bridge_connect_immune(
    pink_immune_bridge_t* bridge,
    brain_immune_system_t* immune
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_connect_immune: bridge is NULL");
        return -1;
    }
    bridge->immune_system = immune;
    NIMCP_LOGGING_DEBUG("Connected to brain immune system");
    return 0;
}

int pink_immune_bridge_connect_generator(
    pink_immune_bridge_t* bridge,
    pink_noise_generator_t generator
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_connect_generator: bridge is NULL");
        return -1;
    }
    bridge->noise_generator = generator;
    NIMCP_LOGGING_DEBUG("Connected to pink noise generator");
    return 0;
}

int pink_immune_bridge_disconnect(pink_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_disconnect: bridge is NULL");
        return -1;
    }
    bridge->immune_system = NULL;
    bridge->noise_generator = NULL;
    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int pink_immune_bridge_update_immune_state(pink_immune_bridge_t* bridge) {
    /**
     * WHAT: Pull current state from immune system
     * WHY:  Keep bridge synchronized with immune
     * HOW:  Read cytokine levels and inflammation
     */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_update_immune_state: bridge is NULL");
        return -1;
    }

    // In a full implementation, we would read from the immune system
    // For now, use the manually set values
    // if (bridge->immune_system) {
    //     brain_immune_get_cytokines(bridge->immune_system, ...);
    // }

    return 0;
}

int pink_immune_bridge_set_cytokine(
    pink_immune_bridge_t* bridge,
    pink_immune_cytokine_t cytokine,
    float level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_set_cytokine: bridge is NULL");
        return -1;
    }
    if (cytokine >= PINK_CYTOKINE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_immune_bridge_set_cytokine: invalid cytokine");
        return -1;
    }
    if (level < 0.0f || level > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_immune_bridge_set_cytokine: level out of range");
        return -1;
    }

    bridge->immune_state.levels[cytokine] = level;
    return 0;
}

int pink_immune_bridge_set_inflammation(
    pink_immune_bridge_t* bridge,
    pink_inflammation_level_t level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_set_inflammation: bridge is NULL");
        return -1;
    }
    if (level > PINK_INFLAMMATION_STORM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_immune_bridge_set_inflammation: invalid level");
        return -1;
    }

    bridge->immune_state.inflammation = level;
    return 0;
}

int pink_immune_bridge_compute_effects(pink_immune_bridge_t* bridge) {
    /**
     * WHAT: Calculate noise modifiers from immune state
     * WHY:  Determine how immune affects noise
     * HOW:  Combine cytokine and inflammation effects
     */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_compute_effects: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_immune_modulation) {
        bridge->effects.amplitude_modifier = 1.0f;
        bridge->effects.alpha_modifier = 0.0f;
        return 0;
    }

    float amp_mod = 1.0f;
    float alpha_mod = 0.0f;

    // Apply cytokine effects
    for (int i = 0; i < PINK_CYTOKINE_COUNT; i++) {
        float level = bridge->immune_state.levels[i];
        float threshold = bridge->config.cytokine_effects[i].threshold;

        if (level > threshold) {
            float effect_strength = (level - threshold) / (1.0f - threshold);

            // IL-10 (anti-inflammatory) counters others
            if (i == PINK_CYTOKINE_IL10) {
                amp_mod *= 1.0f - effect_strength *
                          (1.0f - bridge->config.cytokine_effects[i].amplitude_factor);
            } else {
                amp_mod *= 1.0f + effect_strength *
                          (bridge->config.cytokine_effects[i].amplitude_factor - 1.0f);
            }

            alpha_mod += effect_strength * bridge->config.cytokine_effects[i].alpha_shift;
        }
    }

    // Apply inflammation level effect
    pink_inflammation_level_t inf_level = bridge->immune_state.inflammation;
    amp_mod *= bridge->config.inflammation_amplitude_scale[inf_level];
    alpha_mod += bridge->config.inflammation_alpha_shift[inf_level];

    // Store results
    bridge->effects.amplitude_modifier = amp_mod;
    bridge->effects.alpha_modifier = alpha_mod;
    bridge->effects.effective_amplitude = bridge->config.base_amplitude * amp_mod;
    bridge->effects.effective_alpha = bridge->config.base_alpha + alpha_mod;

    // Clamp effective alpha to reasonable range
    bridge->effects.effective_alpha = fmaxf(0.2f, fminf(2.0f, bridge->effects.effective_alpha));

    // Update running averages
    bridge->update_count++;
    float w = 1.0f / (float)bridge->update_count;
    bridge->avg_amplitude_modifier = (1.0f - w) * bridge->avg_amplitude_modifier + w * amp_mod;
    if (!isfinite(bridge->avg_amplitude_modifier)) bridge->avg_amplitude_modifier = amp_mod;
    bridge->avg_alpha_modifier = (1.0f - w) * bridge->avg_alpha_modifier + w * alpha_mod;
    if (!isfinite(bridge->avg_alpha_modifier)) bridge->avg_alpha_modifier = alpha_mod;

    return 0;
}

int pink_immune_bridge_compute_feedback(
    pink_immune_bridge_t* bridge,
    float measured_alpha,
    float variance
) {
    /**
     * WHAT: Compute signals to send to immune system
     * WHY:  Inform immune about neural state abnormalities
     */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_compute_feedback: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_noise_feedback) return 0;

    // Compute alpha deviation
    float target_alpha = bridge->config.base_alpha;
    bridge->feedback.alpha_deviation = fabsf(measured_alpha - target_alpha);

    // Variance level
    bridge->feedback.variance_level = variance;

    // Seizure warning (excessive variance)
    bridge->feedback.seizure_warning = (variance > bridge->config.variance_warning_threshold);

    // Criticality stress (departure from optimal α=1)
    float optimal_alpha = 1.0f;  // Pink noise is optimal
    bridge->feedback.criticality_stress = fabsf(measured_alpha - optimal_alpha) *
                                          bridge->config.feedback_gain;

    return 0;
}

int pink_immune_bridge_update(
    pink_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /**
     * WHAT: Full update cycle
     * WHY:  Single call for complete bidirectional update
     */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_update: bridge is NULL");
        return -1;
    }

    (void)delta_ms;  // May be used for time-dependent effects

    // Update from immune system
    int result = pink_immune_bridge_update_immune_state(bridge);
    if (result != 0) return result;

    // Compute effects on noise
    result = pink_immune_bridge_compute_effects(bridge);
    if (result != 0) return result;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

float pink_immune_bridge_get_amplitude_modifier(
    const pink_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;
    return bridge->effects.amplitude_modifier;
}

float pink_immune_bridge_get_alpha_modifier(
    const pink_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;
    return bridge->effects.alpha_modifier;
}

float pink_immune_bridge_get_effective_amplitude(
    const pink_immune_bridge_t* bridge
) {
    if (!bridge) return 0.05f;
    return bridge->effects.effective_amplitude;
}

float pink_immune_bridge_get_effective_alpha(
    const pink_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;
    return bridge->effects.effective_alpha;
}

int pink_immune_bridge_get_feedback(
    const pink_immune_bridge_t* bridge,
    pink_immune_feedback_t* feedback
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_get_feedback: bridge is NULL");
        return -1;
    }
    if (!feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_get_feedback: feedback is NULL");
        return -1;
    }
    memcpy(feedback, &bridge->feedback, sizeof(pink_immune_feedback_t));
    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int pink_immune_bridge_get_stats(
    const pink_immune_bridge_t* bridge,
    pink_immune_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_get_stats: stats is NULL");
        return -1;
    }

    memset(stats, 0, sizeof(pink_immune_stats_t));
    stats->total_updates = bridge->update_count;
    stats->avg_amplitude_modifier = bridge->avg_amplitude_modifier;
    stats->avg_alpha_modifier = bridge->avg_alpha_modifier;
    stats->current_inflammation = (float)bridge->immune_state.inflammation;

    return 0;
}

int pink_immune_bridge_reset(pink_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_reset: bridge is NULL");
        return -1;
    }

    memset(&bridge->immune_state, 0, sizeof(pink_immune_state_t));
    bridge->effects.amplitude_modifier = 1.0f;
    bridge->effects.alpha_modifier = 0.0f;
    bridge->effects.effective_amplitude = bridge->config.base_amplitude;
    bridge->effects.effective_alpha = bridge->config.base_alpha;

    memset(&bridge->feedback, 0, sizeof(pink_immune_feedback_t));

    bridge->update_count = 0;
    bridge->avg_amplitude_modifier = 1.0f;
    bridge->avg_alpha_modifier = 0.0f;

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int pink_immune_bridge_connect_bio_async(pink_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect to bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_INFO("Pink noise-immune bridge already connected to bio-async");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_PINK_NOISE,
        .module_name = "pink_noise_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Pink noise-immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

int pink_immune_bridge_disconnect_bio_async(pink_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect from bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_immune_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Pink noise-immune bridge disconnected from bio-async router");

    return 0;
}

bool pink_immune_bridge_is_bio_async_connected(const pink_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;  /* Boolean query returning false for NULL is normal */
    }
    return bridge->base.bio_async_enabled;
}
