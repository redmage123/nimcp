/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * sfa_pink_noise_create has zero callers in production code.
 * brain_cycle_coordinator stores a `pink_noise_bridge` pointer but
 * never calls update on it. The wrapper is a statue around
 * pink_noise_create (which itself is wired elsewhere). Either wire a
 * consumer or delete before the next major version. Do not extend.
 */

#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sfa_pink_noise_bridge)

#define LOG_MODULE "SFA_PINK_NOISE_BRIDGE"

//=============================================================================
// SFA Pink Noise Bridge Implementation
//=============================================================================

#include "core/neuron_models/nimcp_sfa_pink_noise_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/math/nimcp_math_helpers.h"

sfa_pink_noise_bridge_t* sfa_pink_noise_create(const sfa_pink_noise_config_t* config) {
    sfa_pink_noise_bridge_t* bridge = (sfa_pink_noise_bridge_t*)nimcp_calloc(1, sizeof(sfa_pink_noise_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sfa_pink_noise_create: failed to allocate bridge");
        return NULL;
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "sfa_pink_noise_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "sfa_pink_noise_create: validation failed");
        return NULL;
    }

    /* Apply config or defaults */
    bridge->config = config ? *config : sfa_pink_noise_default_config();

    /* Create pink noise generator from SFA config */
    pink_noise_config_t pn_config = pink_noise_default_config();
    pn_config.alpha = bridge->config.noise_alpha;
    pn_config.amplitude = bridge->config.noise_amplitude;
    pn_config.seed = bridge->config.seed;

    bridge->noise_gen = pink_noise_create(&pn_config);
    if (!bridge->noise_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sfa_pink_noise_create: failed to create pink noise generator");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }

    /* Set initial SFA parameter values */
    bridge->noisy_tau = SFA_DEFAULT_TAU;
    bridge->noisy_strength = SFA_DEFAULT_STRENGTH;
    bridge->noisy_threshold = 0.0f;

    bridge->tau_noise = 0.0f;
    bridge->strength_noise = 0.0f;
    bridge->threshold_noise = 0.0f;

    bridge->samples_generated = 0;
    bridge->is_enabled = bridge->config.enabled;
    bridge->neuron_connected = false;
    bridge->noise_connected = true;
    bridge->neuron_state = NULL;

    sfa_pink_noise_bridge_heartbeat("create", 1.0f);
    NIMCP_LOGGING_INFO("Created %s bridge", "sfa_pink_noise");
    return bridge;
}

void sfa_pink_noise_destroy(sfa_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sfa_pink_noise_destroy: bridge is NULL");
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "sfa_pink_noise");
    }

    sfa_pink_noise_bridge_heartbeat("destroy", 0.0f);

    /* Destroy pink noise generator */
    if (bridge->noise_gen) {
        pink_noise_destroy(bridge->noise_gen);
        bridge->noise_gen = NULL;
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
}

int sfa_pink_noise_connect(sfa_pink_noise_bridge_t* bridge, void* neuron_state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sfa_pink_noise_connect: bridge is NULL");
        return -1;
    }
    if (!neuron_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sfa_pink_noise_connect: neuron_state is NULL");
        return -1;
    }

    bridge->neuron_state = neuron_state;
    bridge->neuron_connected = true;

    sfa_pink_noise_bridge_heartbeat("connect", 1.0f);
    return 0;
}

int sfa_pink_noise_update(sfa_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sfa_pink_noise_update: bridge is NULL");
        return -1;
    }

    if (!bridge->is_enabled) {
        return 0;
    }

    if (!bridge->noise_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "sfa_pink_noise_update: noise generator is NULL");
        return -1;
    }

    /* Generate a pink noise sample */
    float noise_sample = 0.0f;
    if (!pink_noise_generate_sample(bridge->noise_gen, &noise_sample)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "sfa_pink_noise_update: failed to generate noise sample");
        return -1;
    }

    /* Modulate SFA tau using pink noise */
    bridge->tau_noise = noise_sample * bridge->config.tau_noise_scale;
    bridge->noisy_tau = SFA_DEFAULT_TAU * (1.0f + bridge->tau_noise);
    bridge->noisy_tau = nimcp_clampf(bridge->noisy_tau, bridge->config.tau_min, bridge->config.tau_max);

    /* Modulate SFA strength using pink noise (generate next sample for decorrelation) */
    float strength_sample = 0.0f;
    if (!pink_noise_generate_sample(bridge->noise_gen, &strength_sample)) {
        strength_sample = noise_sample * 0.7f;  /* Fallback: scale from tau noise */
    }
    bridge->strength_noise = strength_sample * bridge->config.strength_noise_scale;
    bridge->noisy_strength = SFA_DEFAULT_STRENGTH * (1.0f + bridge->strength_noise);
    bridge->noisy_strength = nimcp_clampf(bridge->noisy_strength, bridge->config.strength_min, bridge->config.strength_max);

    /* Modulate threshold using pink noise */
    float threshold_sample = 0.0f;
    if (!pink_noise_generate_sample(bridge->noise_gen, &threshold_sample)) {
        threshold_sample = noise_sample * 0.5f;  /* Fallback: scale from tau noise */
    }
    bridge->threshold_noise = threshold_sample * bridge->config.threshold_noise_scale;
    bridge->noisy_threshold = bridge->threshold_noise;

    bridge->samples_generated++;

    /* Record update in base */
    bridge_base_record_update(&bridge->base);

    sfa_pink_noise_bridge_heartbeat("update", 1.0f);
    return 0;
}

float sfa_pink_noise_get_tau(const sfa_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sfa_pink_noise_get_tau: bridge is NULL");
        return SFA_DEFAULT_TAU;
    }
    return bridge->noisy_tau;
}

float sfa_pink_noise_get_strength(const sfa_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sfa_pink_noise_get_strength: bridge is NULL");
        return SFA_DEFAULT_STRENGTH;
    }
    return bridge->noisy_strength;
}

int sfa_pink_noise_reset(sfa_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sfa_pink_noise_reset: bridge is NULL");
        return -1;
    }

    /* Reset noise state to zero */
    bridge->tau_noise = 0.0f;
    bridge->strength_noise = 0.0f;
    bridge->threshold_noise = 0.0f;

    /* Reset noisy parameters to defaults */
    bridge->noisy_tau = SFA_DEFAULT_TAU;
    bridge->noisy_strength = SFA_DEFAULT_STRENGTH;
    bridge->noisy_threshold = 0.0f;

    bridge->samples_generated = 0;

    /* Reset the pink noise generator */
    if (bridge->noise_gen) {
        pink_noise_reset(bridge->noise_gen, 0);
    }

    /* Reset base bridge state */
    bridge_base_reset(&bridge->base);

    sfa_pink_noise_bridge_heartbeat("reset", 1.0f);
    return 0;
}
