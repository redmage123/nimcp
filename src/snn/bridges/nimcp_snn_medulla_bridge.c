/**
 * @file nimcp_snn_medulla_bridge.c
 * @brief SNN-Medulla integration bridge implementation
 *
 * WHAT: Bidirectional integration between SNN and medulla oblongata
 * WHY:  Enable spike-based autonomic regulation and brainstem modulation
 * HOW:  Arousal/protection/circadian states modulate SNN activity
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_medulla_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_medulla_bridge)

//=============================================================================
// Constants
//=============================================================================

#define SNN_MEDULLA_BRIDGE_MODULE_ID 0x0D50

//=============================================================================
// Lifecycle Functions
//=============================================================================

void snn_medulla_config_default(snn_medulla_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(snn_medulla_config_t));

    /* Arousal modulation */
    config->arousal_min_rate_factor = 0.1f;
    config->arousal_max_rate_factor = 2.0f;
    config->arousal_excitability_gain = 0.5f;

    /* Protection effects */
    config->protection_throttle_factor = 0.5f;
    config->protection_shed_factor = 0.2f;
    config->protection_safe_factor = 0.05f;
    config->enable_emergency_shutdown = true;

    /* Circadian modulation */
    config->circadian_amplitude = 0.3f;
    config->circadian_peak_phase = (float)CIRCADIAN_PHASE_MORNING;
    config->enable_circadian_modulation = true;

    /* Activity feedback */
    config->activity_threat_threshold = 100.0f;
    config->activity_emergency_threshold = 200.0f;
    config->enable_activity_feedback = true;

    /* Population configuration */
    config->arousal_sensing_pop_id = 0;
    config->motor_output_pop_id = 0;

    /* Update timing */
    config->update_interval_ms = 10.0f;

    /* Bio-async */
    config->enable_bio_async = true;
}

snn_medulla_bridge_t* snn_medulla_bridge_create(
    const snn_medulla_config_t* config,
    snn_network_t* snn,
    medulla_t medulla
) {
    if (!snn) {
        NIMCP_LOGGING_WARN("Null SNN to snn_medulla_bridge_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_medulla_bridge_create: snn is NULL");
        return NULL;
    }

    snn_medulla_bridge_t* bridge = nimcp_calloc(1, sizeof(snn_medulla_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-medulla bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_medulla_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        snn_medulla_config_default(&bridge->config);
    }

    bridge->snn = snn;
    bridge->medulla = medulla;

    /* Initialize state */
    memset(&bridge->state, 0, sizeof(snn_medulla_state_t));
    bridge->state.arousal_rate_factor = 1.0f;
    bridge->state.protection_gate = 1.0f;
    bridge->state.combined_modulation = 1.0f;

    bridge->last_update_time = 0.0f;
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "snn_medulla") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_WARN("Failed to create mutex for SNN-medulla bridge");
    }

    return bridge;
}

void snn_medulla_bridge_destroy(snn_medulla_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        snn_medulla_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

//=============================================================================
// Bio-async Integration
//=============================================================================

int snn_medulla_bridge_connect_bio_async(snn_medulla_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_medulla_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = SNN_MEDULLA_BRIDGE_MODULE_ID,
        .module_name = "snn_medulla_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("SNN-medulla bridge connected to bio-async");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available for SNN-medulla bridge");
        return -2;
    }
}

int snn_medulla_bridge_disconnect_bio_async(snn_medulla_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_medulla_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_medulla_bridge_is_bio_async_connected(const snn_medulla_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_medulla_bridge_is_bio_async_connected: bridge is NULL");
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Connection Functions
//=============================================================================

int snn_medulla_bridge_connect_medulla(
    snn_medulla_bridge_t* bridge,
    medulla_t medulla
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_medulla_bridge_connect_medulla: bridge is NULL");
        return -1;
    }

    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->medulla = medulla;
    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Modulation Functions
//=============================================================================

float snn_medulla_compute_arousal_modulation(const snn_medulla_bridge_t* bridge) {
    if (!bridge || !bridge->medulla) return 1.0f;

    float arousal = medulla_get_arousal_level(bridge->medulla);
    if (arousal < 0.0f) arousal = 0.5f;  /* Default if error */

    /* Map arousal [0,1] to rate factor [min, max] */
    float range = bridge->config.arousal_max_rate_factor -
                  bridge->config.arousal_min_rate_factor;
    float factor = bridge->config.arousal_min_rate_factor + arousal * range;

    return factor;
}

float snn_medulla_compute_protection_gate(const snn_medulla_bridge_t* bridge) {
    if (!bridge || !bridge->medulla) return 1.0f;

    protection_level_t level = medulla_get_protection_level(bridge->medulla);

    switch (level) {
        case PROTECTION_LEVEL_NORMAL:
        case PROTECTION_LEVEL_CAUTIOUS:
            return 1.0f;
        case PROTECTION_LEVEL_GUARDED:
            return bridge->config.protection_throttle_factor;
        case PROTECTION_LEVEL_DEFENSIVE:
            return bridge->config.protection_shed_factor;
        case PROTECTION_LEVEL_CRITICAL:
            return bridge->config.protection_safe_factor;
        case PROTECTION_LEVEL_SHUTDOWN:
            return 0.0f;
        default:
            return 1.0f;
    }
}

float snn_medulla_compute_circadian_modulation(const snn_medulla_bridge_t* bridge) {
    if (!bridge || !bridge->medulla) return 0.0f;
    if (!bridge->config.enable_circadian_modulation) return 0.0f;

    circadian_phase_t phase = medulla_get_circadian_phase(bridge->medulla);
    float peak = bridge->config.circadian_peak_phase;
    float amplitude = bridge->config.circadian_amplitude;

    /* Cosine modulation centered on peak phase */
    float phase_diff = (float)phase - peak;
    /* 8 phases in a day, so full cycle = 8 */
    float angle = (phase_diff / 4.0f) * NIMCP_PI_F;
    float modulation = amplitude * cosf(angle);

    return modulation;
}

int snn_medulla_apply_modulation(snn_medulla_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_medulla_apply_modulation: bridge is NULL");
        return -1;
    }
    if (!bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_medulla_apply_modulation: bridge->snn is NULL");
        return -1;
    }

    /* Compute combined modulation */
    float arousal_mod = snn_medulla_compute_arousal_modulation(bridge);
    float protection_gate = snn_medulla_compute_protection_gate(bridge);
    float circadian_mod = snn_medulla_compute_circadian_modulation(bridge);

    /* Combined: arousal * protection_gate * (1 + circadian) */
    float combined = arousal_mod * protection_gate * (1.0f + circadian_mod);

    /* Clamp to reasonable range */
    if (combined < 0.0f) combined = 0.0f;
    if (combined > 3.0f) combined = 3.0f;

    /* Store in state */
    bridge->state.arousal_rate_factor = arousal_mod;
    bridge->state.protection_gate = protection_gate;
    bridge->state.circadian_modulation = circadian_mod;
    bridge->state.combined_modulation = combined;

    /* Update running average */
    bridge->state.avg_modulation =
        0.95f * bridge->state.avg_modulation + 0.05f * combined;

    /* TODO: Actually apply modulation to SNN populations when API available */
    /* For now, modulation values are computed and stored for external use */

    return 0;
}

//=============================================================================
// Feedback Functions
//=============================================================================

int snn_medulla_check_activity_threat(snn_medulla_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_medulla_check_activity_threat: bridge is NULL");
        return -1;
    }
    if (!bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_medulla_check_activity_threat: bridge->snn is NULL");
        return -1;
    }
    if (!bridge->config.enable_activity_feedback) return 0;
    if (!bridge->medulla) return 0;

    /* Get average firing rate from SNN (placeholder - actual implementation
       would query SNN population statistics) */
    float avg_rate = bridge->state.avg_firing_rate_hz;
    float max_rate = bridge->state.max_firing_rate_hz;

    int result = 0;

    /* Check for emergency threshold */
    if (max_rate >= bridge->config.activity_emergency_threshold) {
        if (!bridge->state.emergency_triggered) {
            snn_medulla_trigger_emergency(bridge, "SNN activity exceeded emergency threshold");
            bridge->state.emergency_triggered = true;
            bridge->state.emergency_count++;
            result = 2;
        }
    }
    /* Check for threat threshold */
    else if (avg_rate >= bridge->config.activity_threat_threshold) {
        if (!bridge->state.activity_threat_triggered) {
            bridge->state.activity_threat_triggered = true;
            bridge->state.protection_activations++;
            result = 1;
        }
    }
    /* Reset triggers if activity subsides */
    else {
        bridge->state.activity_threat_triggered = false;
        bridge->state.emergency_triggered = false;
    }

    return result;
}

int snn_medulla_trigger_emergency(
    snn_medulla_bridge_t* bridge,
    const char* reason
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_medulla_trigger_emergency: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_emergency_shutdown) return 0;
    if (!bridge->medulla) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_medulla_trigger_emergency: bridge->medulla is NULL");
        return -2;
    }

    NIMCP_LOGGING_WARN("SNN triggering medulla emergency: %s", reason ? reason : "unknown");

    return medulla_emergency_shutdown(bridge->medulla, reason);
}

//=============================================================================
// Update Functions
//=============================================================================

int snn_medulla_bridge_update(snn_medulla_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_medulla_bridge_update: bridge is NULL");
        return -1;
    }

    bridge->last_update_time += dt;

    /* Check if update interval elapsed */
    if (bridge->last_update_time < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time = 0.0f;

    if (bridge->base.mutex) nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Read current medulla state */
    if (bridge->medulla) {
        bridge->state.current_arousal = medulla_get_arousal_level(bridge->medulla);
        bridge->state.protection_level = medulla_get_protection_level(bridge->medulla);
        bridge->state.circadian_phase = medulla_get_circadian_phase(bridge->medulla);

        /* Map float arousal to discrete level */
        if (bridge->state.current_arousal < 0.1f) {
            bridge->state.arousal_level = AROUSAL_LEVEL_COMA;
        } else if (bridge->state.current_arousal < 0.2f) {
            bridge->state.arousal_level = AROUSAL_LEVEL_DEEP_SLEEP;
        } else if (bridge->state.current_arousal < 0.35f) {
            bridge->state.arousal_level = AROUSAL_LEVEL_LIGHT_SLEEP;
        } else if (bridge->state.current_arousal < 0.45f) {
            bridge->state.arousal_level = AROUSAL_LEVEL_DROWSY;
        } else if (bridge->state.current_arousal < 0.65f) {
            bridge->state.arousal_level = AROUSAL_LEVEL_AWAKE;
        } else if (bridge->state.current_arousal < 0.85f) {
            bridge->state.arousal_level = AROUSAL_LEVEL_ALERT;
        } else {
            bridge->state.arousal_level = AROUSAL_LEVEL_HYPERAROUSAL;
        }
    }

    /* Apply modulation to SNN */
    snn_medulla_apply_modulation(bridge);

    /* Check for activity threats */
    snn_medulla_check_activity_threat(bridge);

    bridge->state.sync_count++;

    if (bridge->base.mutex) nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int snn_medulla_bridge_get_state(
    const snn_medulla_bridge_t* bridge,
    snn_medulla_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_medulla_bridge_get_state: bridge is NULL");
        return -1;
    }
    if (state) {
        *state = bridge->state;
    }
    return 0;
}

float snn_medulla_get_arousal(const snn_medulla_bridge_t* bridge) {
    if (!bridge) return -1.0f;
    return bridge->state.current_arousal;
}

protection_level_t snn_medulla_get_protection_level(const snn_medulla_bridge_t* bridge) {
    if (!bridge) return PROTECTION_LEVEL_NORMAL;
    return bridge->state.protection_level;
}

circadian_phase_t snn_medulla_get_circadian_phase(const snn_medulla_bridge_t* bridge) {
    if (!bridge) return CIRCADIAN_PHASE_MORNING;
    return bridge->state.circadian_phase;
}

float snn_medulla_get_combined_modulation(const snn_medulla_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->state.combined_modulation;
}

bool snn_medulla_is_activity_restricted(const snn_medulla_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->state.protection_level >= PROTECTION_LEVEL_GUARDED;
}

//=============================================================================
// Statistics Functions
//=============================================================================

int snn_medulla_get_stats(
    const snn_medulla_bridge_t* bridge,
    uint32_t* sync_count,
    uint32_t* emergency_count,
    float* avg_modulation
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_medulla_get_stats: bridge is NULL");
        return -1;
    }

    if (sync_count) *sync_count = bridge->state.sync_count;
    if (emergency_count) *emergency_count = bridge->state.emergency_count;
    if (avg_modulation) *avg_modulation = bridge->state.avg_modulation;

    return 0;
}

void snn_medulla_reset_stats(snn_medulla_bridge_t* bridge) {
    if (!bridge) return;

    bridge->state.sync_count = 0;
    bridge->state.emergency_count = 0;
    bridge->state.protection_activations = 0;
    bridge->state.avg_modulation = 1.0f;
    bridge->state.activity_threat_triggered = false;
    bridge->state.emergency_triggered = false;
}
