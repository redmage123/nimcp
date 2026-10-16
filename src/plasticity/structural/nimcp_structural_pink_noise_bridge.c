/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * structural_pink_noise_create has zero callers in production code.
 * Wrapper around pink_noise_create that is unused. Either wire a
 * consumer or delete before the next major version. Do not extend.
 */

#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_structural_pink_noise_bridge.c - Pink Noise for Structural Plasticity
//=============================================================================

#include "plasticity/structural/nimcp_structural_pink_noise_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(structural_pink_noise_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(struct_pink_noise_bridge)

struct_pink_noise_bridge_t* struct_pink_noise_create(const struct_pink_noise_config_t* config) {
    struct_pink_noise_bridge_t* bridge = nimcp_calloc(1, sizeof(struct_pink_noise_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "struct_pink_noise_create: bridge allocation failed");
        return NULL;
    }

    bridge->config = config ? *config : struct_pink_noise_default_config();

    pink_noise_config_t nc;
    pink_noise_default_config(&nc);
    nc.alpha = bridge->config.noise_alpha;
    nc.amplitude = bridge->config.noise_amplitude;
    nc.seed = bridge->config.seed;

    bridge->noise_gen = pink_noise_create(&nc);
    if (!bridge->noise_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "struct_pink_noise_create: noise_gen allocation failed");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->noise_connected = true;
    bridge->is_enabled = bridge->config.enabled;
    bridge->noisy_growth_rate = 0.01f;
    bridge->noisy_prune_rate = 0.005f;

    NIMCP_LOGGING_INFO("Created structural pink noise bridge");
    return bridge;
}

void struct_pink_noise_destroy(struct_pink_noise_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->noise_gen) pink_noise_destroy(bridge->noise_gen);
    nimcp_free(bridge);
}

int struct_pink_noise_connect(struct_pink_noise_bridge_t* bridge, void* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "struct_pink_noise_connect: bridge is NULL");
        return -1;
    }
    bridge->structural_state = state;
    bridge->struct_connected = (state != NULL);
    return 0;
}

int struct_pink_noise_update(struct_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "struct_pink_noise_update: bridge is NULL");
        return -1;
    }
    if (!bridge->is_enabled) return 0;  /* Not an error, just disabled */
    if (!bridge->noise_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "struct_pink_noise_update: noise_gen is NULL");
        return -1;
    }

    float amp = bridge->config.noise_amplitude;

    bridge->growth_noise = pink_noise_sample(bridge->noise_gen) * bridge->config.growth_noise_scale;
    bridge->prune_noise = pink_noise_sample(bridge->noise_gen) * bridge->config.prune_noise_scale;

    bridge->noisy_growth_rate = 0.01f * (1.0f + amp * bridge->growth_noise);
    bridge->noisy_prune_rate = 0.005f * (1.0f + amp * bridge->prune_noise);

    bridge->noisy_growth_rate = nimcp_clampf(bridge->noisy_growth_rate, 0.001f, 0.1f);
    bridge->noisy_prune_rate = nimcp_clampf(bridge->noisy_prune_rate, 0.0001f, 0.05f);

    bridge->samples_generated++;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

float struct_pink_noise_get_growth_rate(const struct_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "struct_pink_noise_get_growth_rate: bridge is NULL");
        return 0.01f;
    }
    return bridge->noisy_growth_rate;
}

float struct_pink_noise_get_prune_rate(const struct_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "struct_pink_noise_get_prune_rate: bridge is NULL");
        return 0.005f;
    }
    return bridge->noisy_prune_rate;
}

int struct_pink_noise_reset(struct_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "struct_pink_noise_reset: bridge is NULL");
        return -1;
    }
    bridge->growth_noise = bridge->prune_noise = 0.0f;
    bridge->noisy_growth_rate = 0.01f;
    bridge->noisy_prune_rate = 0.005f;
    bridge->samples_generated = 0;
    if (bridge->noise_gen) pink_noise_reset(bridge->noise_gen);
    return 0;
}
