/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * metaplasticity_pink_noise_create has zero callers in production
 * code. Wrapper around pink_noise_create that is unused. Either wire
 * a consumer or delete before the next major version. Do not extend.
 */

#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_metaplasticity_pink_noise_bridge.c - Pink Noise for Metaplasticity
//=============================================================================

#include "plasticity/metaplasticity/nimcp_metaplasticity_pink_noise_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(metaplasticity_pink_noise_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(meta_pink_noise_bridge)

meta_pink_noise_bridge_t* meta_pink_noise_create(const meta_pink_noise_config_t* config) {
    meta_pink_noise_bridge_t* bridge = nimcp_calloc(1, sizeof(meta_pink_noise_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_pink_noise_create: failed to allocate bridge");
        return NULL;
    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(meta_pink_noise_config_t));
    } else {
        bridge->config = meta_pink_noise_default_config();
    }

    pink_noise_config_t noise_config;
    pink_noise_default_config(&noise_config);
    noise_config.alpha = bridge->config.noise_alpha;
    noise_config.amplitude = bridge->config.noise_amplitude;
    noise_config.seed = bridge->config.seed;

    bridge->noise_gen = pink_noise_create(&noise_config);
    if (!bridge->noise_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_pink_noise_create: failed to create noise generator");
        nimcp_free(bridge);
        return NULL;
    }
    bridge->noise_connected = true;
    bridge->is_enabled = bridge->config.enabled;
    bridge->noisy_theta_baseline = META_PINK_NOISE_DEFAULT_THETA;
    bridge->noisy_theta_effective = META_PINK_NOISE_DEFAULT_THETA;
    bridge->noisy_baseline_tau = METAPLASTICITY_DEFAULT_BASELINE_TAU;
    bridge->noisy_history_tau = METAPLASTICITY_DEFAULT_HISTORY_TAU;

    NIMCP_LOGGING_INFO("Created metaplasticity pink noise bridge");
    return bridge;
}

void meta_pink_noise_destroy(meta_pink_noise_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->noise_gen) pink_noise_destroy(bridge->noise_gen);
    nimcp_free(bridge);
}

int meta_pink_noise_connect_meta(meta_pink_noise_bridge_t* bridge, void* meta_state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_pink_noise_connect_meta: bridge is NULL");
        return -1;
    }
    bridge->metaplasticity_state = meta_state;
    bridge->meta_connected = (meta_state != NULL);
    return 0;
}

int meta_pink_noise_disconnect(meta_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_pink_noise_disconnect: bridge is NULL");
        return -1;
    }
    bridge->metaplasticity_state = NULL;
    bridge->meta_connected = false;
    return 0;
}

int meta_pink_noise_update(meta_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_pink_noise_update: bridge is NULL");
        return -1;
    }
    if (!bridge->is_enabled) {
        return -1;  /* Not an error, just disabled */
    }
    if (!bridge->noise_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "meta_pink_noise_update: noise_gen is NULL");
        return -1;
    }

    if (bridge->config.noise_targets & META_NOISE_TARGET_THETA_BASE) {
        bridge->theta_base_noise = pink_noise_sample(bridge->noise_gen) *
            bridge->config.theta_base_noise_scale;
    }
    if (bridge->config.noise_targets & META_NOISE_TARGET_THETA_EFF) {
        bridge->theta_eff_noise = pink_noise_sample(bridge->noise_gen) *
            bridge->config.theta_eff_noise_scale;
    }
    if (bridge->config.noise_targets & (META_NOISE_TARGET_TAU_BASE | META_NOISE_TARGET_TAU_HIST)) {
        bridge->tau_noise = pink_noise_sample(bridge->noise_gen) *
            bridge->config.tau_noise_scale;
    }

    float amp = bridge->config.noise_amplitude;
    float base_theta = META_PINK_NOISE_DEFAULT_THETA;
    float base_tau = METAPLASTICITY_DEFAULT_BASELINE_TAU;
    float base_hist_tau = METAPLASTICITY_DEFAULT_HISTORY_TAU;

    bridge->noisy_theta_baseline = base_theta * (1.0f + amp * bridge->theta_base_noise);
    bridge->noisy_theta_effective = base_theta * (1.0f + amp * bridge->theta_eff_noise);
    bridge->noisy_baseline_tau = base_tau * (1.0f + amp * bridge->tau_noise);
    bridge->noisy_history_tau = base_hist_tau * (1.0f + amp * bridge->tau_noise);

    bridge->noisy_theta_baseline = nimcp_clampf(bridge->noisy_theta_baseline,
        bridge->config.theta_min, bridge->config.theta_max);
    bridge->noisy_theta_effective = nimcp_clampf(bridge->noisy_theta_effective,
        bridge->config.theta_min, bridge->config.theta_max);
    bridge->noisy_baseline_tau = nimcp_clampf(bridge->noisy_baseline_tau,
        bridge->config.tau_min, bridge->config.tau_max);
    bridge->noisy_history_tau = nimcp_clampf(bridge->noisy_history_tau,
        bridge->config.tau_min, bridge->config.tau_max);

    bridge->samples_generated++;
    bridge->parameters_modulated++;
    float noise_amp = fabsf(bridge->theta_base_noise) * amp;
    bridge->avg_noise_amplitude = bridge->avg_noise_amplitude * 0.99f + noise_amp * 0.01f;
    if (!isfinite(bridge->avg_noise_amplitude)) bridge->avg_noise_amplitude = noise_amp;
    if (noise_amp > bridge->max_noise_amplitude) bridge->max_noise_amplitude = noise_amp;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

float meta_pink_noise_get_noisy_theta_baseline(const meta_pink_noise_bridge_t* bridge) {
    return bridge ? bridge->noisy_theta_baseline : META_PINK_NOISE_DEFAULT_THETA;
}

float meta_pink_noise_get_noisy_theta_effective(const meta_pink_noise_bridge_t* bridge) {
    return bridge ? bridge->noisy_theta_effective : META_PINK_NOISE_DEFAULT_THETA;
}

int meta_pink_noise_get_stats(const meta_pink_noise_bridge_t* bridge, meta_pink_noise_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_pink_noise_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_pink_noise_get_stats: stats is NULL");
        return -1;
    }
    memset(stats, 0, sizeof(meta_pink_noise_stats_t));
    stats->samples_generated = bridge->samples_generated;
    stats->parameters_modulated = bridge->parameters_modulated;
    stats->avg_noise_amplitude = bridge->avg_noise_amplitude;
    stats->max_noise_amplitude = bridge->max_noise_amplitude;
    stats->current_noisy_theta_base = bridge->noisy_theta_baseline;
    stats->current_noisy_theta_eff = bridge->noisy_theta_effective;
    return 0;
}

int meta_pink_noise_set_enabled(meta_pink_noise_bridge_t* bridge, bool enabled) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_pink_noise_set_enabled: bridge is NULL");
        return -1;
    }
    bridge->is_enabled = enabled;
    bridge->config.enabled = enabled;
    return 0;
}

int meta_pink_noise_reset(meta_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_pink_noise_reset: bridge is NULL");
        return -1;
    }
    bridge->theta_base_noise = 0.0f;
    bridge->theta_eff_noise = 0.0f;
    bridge->tau_noise = 0.0f;
    bridge->noisy_theta_baseline = META_PINK_NOISE_DEFAULT_THETA;
    bridge->noisy_theta_effective = META_PINK_NOISE_DEFAULT_THETA;
    bridge->noisy_baseline_tau = METAPLASTICITY_DEFAULT_BASELINE_TAU;
    bridge->noisy_history_tau = METAPLASTICITY_DEFAULT_HISTORY_TAU;
    bridge->samples_generated = 0;
    bridge->parameters_modulated = 0;
    bridge->avg_noise_amplitude = 0.0f;
    bridge->max_noise_amplitude = 0.0f;
    if (bridge->noise_gen) pink_noise_reset(bridge->noise_gen);
    return 0;
}
