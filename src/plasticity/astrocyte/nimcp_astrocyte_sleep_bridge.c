/**
 * @file nimcp_astrocyte_sleep_bridge.c
 * @brief Sleep-Astrocyte Integration Implementation
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional coupling between sleep/wake and astrocyte systems
 * WHY:  Sleep profoundly affects astrocyte gliotransmitter release
 * HOW:  Monitor sleep state, modulate D-serine, glutamate uptake, ATP/adenosine
 */

#include "plasticity/astrocyte/nimcp_astrocyte_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "security/nimcp_bbb_helpers.h"

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(astrocyte_sleep_bridge)

/* Security integration */
/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct astrocyte_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Linked systems */
    sleep_system_t sleep_system;
    astrocyte_plasticity_t astrocyte_system;

    /* Configuration */
    astrocyte_sleep_config_t config;

    /* Current effects */
    astrocyte_sleep_effects_t effects;

    /* Statistics */
    uint64_t total_updates;
    uint32_t modulation_applications;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(astrocyte_sleep_bridge, struct astrocyte_sleep_bridge_struct)

/* ============================================================================
 * Helper Function Implementations
 * ============================================================================ */

float astrocyte_sleep_get_d_serine_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ASTROCYTE_SLEEP_D_SERINE_AWAKE;
        case SLEEP_STATE_DROWSY:     return ASTROCYTE_SLEEP_D_SERINE_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ASTROCYTE_SLEEP_D_SERINE_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ASTROCYTE_SLEEP_D_SERINE_DEEP_NREM;
        case SLEEP_STATE_REM:        return ASTROCYTE_SLEEP_D_SERINE_REM;
        default:                     return ASTROCYTE_SLEEP_D_SERINE_AWAKE;
    }
}

float astrocyte_sleep_get_uptake_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ASTROCYTE_SLEEP_GLU_UPTAKE_AWAKE;
        case SLEEP_STATE_DROWSY:     return ASTROCYTE_SLEEP_GLU_UPTAKE_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ASTROCYTE_SLEEP_GLU_UPTAKE_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ASTROCYTE_SLEEP_GLU_UPTAKE_DEEP_NREM;
        case SLEEP_STATE_REM:        return ASTROCYTE_SLEEP_GLU_UPTAKE_REM;
        default:                     return ASTROCYTE_SLEEP_GLU_UPTAKE_AWAKE;
    }
}

float astrocyte_sleep_get_atp_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ASTROCYTE_SLEEP_ATP_AWAKE;
        case SLEEP_STATE_DROWSY:     return ASTROCYTE_SLEEP_ATP_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ASTROCYTE_SLEEP_ATP_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ASTROCYTE_SLEEP_ATP_DEEP_NREM;
        case SLEEP_STATE_REM:        return ASTROCYTE_SLEEP_ATP_REM;
        default:                     return ASTROCYTE_SLEEP_ATP_AWAKE;
    }
}

float astrocyte_sleep_get_calcium_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:
        case SLEEP_STATE_DROWSY:
            return ASTROCYTE_SLEEP_CA_WAVE_AWAKE;

        case SLEEP_STATE_LIGHT_NREM:
        case SLEEP_STATE_DEEP_NREM:
            return ASTROCYTE_SLEEP_CA_WAVE_NREM;

        case SLEEP_STATE_REM:
            return ASTROCYTE_SLEEP_CA_WAVE_REM;

        default:
            return ASTROCYTE_SLEEP_CA_WAVE_AWAKE;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int astrocyte_sleep_default_config(astrocyte_sleep_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "Astrocyte-sleep config is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_sleep_default_config: config is NULL");

    /* All modulations enabled by default */
    config->enable_d_serine_modulation = true;
    config->enable_uptake_modulation = true;
    config->enable_atp_modulation = true;
    config->enable_calcium_modulation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

astrocyte_sleep_bridge_t astrocyte_sleep_bridge_create(
    const astrocyte_sleep_config_t* config,
    sleep_system_t sleep_system,
    astrocyte_plasticity_t astrocyte_system
) {
    /* Guard: require both systems */
    NIMCP_API_CHECK_NULL_RET_NULL(sleep_system, "Sleep system is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_sleep_bridge_create: sleep_system is NULL");
    NIMCP_API_CHECK_NULL_RET_NULL(astrocyte_system, "Astrocyte system is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_sleep_bridge_create: astrocyte_system is NULL");

    /* Allocate bridge */
    astrocyte_sleep_bridge_t bridge = (astrocyte_sleep_bridge_t)
        nimcp_malloc(sizeof(struct astrocyte_sleep_bridge_struct));
    if (!bridge) {
        LOG_ERROR("Astrocyte-sleep bridge allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "astrocyte_sleep_bridge_create: bridge allocation failed");
        return NULL;
    }
    memset(bridge, 0, sizeof(struct astrocyte_sleep_bridge_struct));

    /* Link systems */
    bridge->sleep_system = sleep_system;
    bridge->astrocyte_system = astrocyte_system;

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(astrocyte_sleep_config_t));
    } else {
        astrocyte_sleep_default_config(&bridge->config);
    }

    /* Initialize effects to awake state */
    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.d_serine_factor = ASTROCYTE_SLEEP_D_SERINE_AWAKE;
    bridge->effects.glutamate_uptake_factor = ASTROCYTE_SLEEP_GLU_UPTAKE_AWAKE;
    bridge->effects.atp_release_factor = ASTROCYTE_SLEEP_ATP_AWAKE;
    bridge->effects.calcium_wave_factor = ASTROCYTE_SLEEP_CA_WAVE_AWAKE;
    bridge->effects.sleep_pressure = 0.0f;
    bridge->effects.glymphatic_active = false;

    /* Create mutex */
    bridge->base.mutex = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        LOG_ERROR("Astrocyte-sleep bridge mutex allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Astrocyte-sleep bridge mutex allocation failed");
        return NULL;
    }
    nimcp_mutex_init(bridge->base.mutex, NULL);

    NIMCP_LOGGING_INFO("Sleep-astrocyte bridge created successfully");
    return bridge;
}

void astrocyte_sleep_bridge_destroy(astrocyte_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_mutex_destroy(bridge->base.mutex);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    NIMCP_LOGGING_DEBUG("Sleep-astrocyte bridge destroyed");
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int astrocyte_sleep_update(astrocyte_sleep_bridge_t bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-sleep bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_sleep_update: bridge is NULL");
    NIMCP_API_CHECK_NULL(bridge->sleep_system, -1, "Sleep system is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "astrocyte_sleep_update: sleep_system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Query sleep system state */
    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float sleep_pressure = sleep_get_pressure(bridge->sleep_system);

    /* Update effects based on sleep state */
    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = sleep_pressure;

    /* D-serine modulation */
    if (bridge->config.enable_d_serine_modulation) {
        bridge->effects.d_serine_factor =
            astrocyte_sleep_get_d_serine_factor(state) *
            bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength);
    } else {
        bridge->effects.d_serine_factor = 1.0f;
    }

    /* Glutamate uptake modulation */
    if (bridge->config.enable_uptake_modulation) {
        bridge->effects.glutamate_uptake_factor =
            astrocyte_sleep_get_uptake_factor(state) *
            bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength);
    } else {
        bridge->effects.glutamate_uptake_factor = 1.0f;
    }

    /* ATP/adenosine modulation */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.atp_release_factor =
            astrocyte_sleep_get_atp_factor(state) *
            bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength);
    } else {
        bridge->effects.atp_release_factor = 1.0f;
    }

    /* Calcium wave modulation */
    if (bridge->config.enable_calcium_modulation) {
        bridge->effects.calcium_wave_factor =
            astrocyte_sleep_get_calcium_factor(state) *
            bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength);
    } else {
        bridge->effects.calcium_wave_factor = 1.0f;
    }

    /* Glymphatic system active during NREM */
    bridge->effects.glymphatic_active =
        (state == SLEEP_STATE_LIGHT_NREM || state == SLEEP_STATE_DEEP_NREM);

    bridge->total_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int astrocyte_sleep_apply_modulation(astrocyte_sleep_bridge_t bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-sleep bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_sleep_apply_modulation: bridge is NULL");
    NIMCP_API_CHECK_NULL(bridge->astrocyte_system, -1, "Astrocyte system is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "astrocyte_sleep_apply_modulation: astrocyte_system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get number of astrocytes */
    uint32_t num_astrocytes =
        astrocyte_plasticity_get_num_astrocytes(bridge->astrocyte_system);

    /* Apply modulation to each astrocyte */
    for (uint32_t i = 0; i < num_astrocytes; i++) {
        astrocyte_state_t state;
        if (astrocyte_plasticity_get_state(bridge->astrocyte_system, i, &state) != 0) {
            continue;
        }

        /* Modulate D-serine */
        if (bridge->config.enable_d_serine_modulation) {
            float modulated_d_serine = state.d_serine_level *
                                       bridge->effects.d_serine_factor;
            astrocyte_plasticity_release_gliotransmitter(
                bridge->astrocyte_system, i,
                GLIOTRANSMITTER_D_SERINE,
                modulated_d_serine - state.d_serine_level
            );
        }

        /* Modulate ATP (adenosine homeostasis) */
        if (bridge->config.enable_atp_modulation) {
            /* Higher sleep pressure → more ATP/adenosine */
            float atp_adjustment = bridge->effects.sleep_pressure * 0.2f;
            astrocyte_plasticity_release_gliotransmitter(
                bridge->astrocyte_system, i,
                GLIOTRANSMITTER_ATP,
                atp_adjustment
            );
        }

        /* Trigger calcium waves during NREM */
        if (bridge->config.enable_calcium_modulation) {
            if (bridge->effects.glymphatic_active &&
                bridge->effects.calcium_wave_factor > 0.8f) {
                /* Probabilistic calcium wave triggering in NREM */
                if ((i % 10) == 0) {  /* Every 10th astrocyte */
                    astrocyte_plasticity_trigger_calcium_wave(
                        bridge->astrocyte_system, i,
                        bridge->effects.calcium_wave_factor
                    );
                }
            }
        }
    }

    bridge->modulation_applications++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int astrocyte_sleep_get_effects(const astrocyte_sleep_bridge_t bridge,
                                 astrocyte_sleep_effects_t* effects) {
    NIMCP_API_CHECK_NULL(bridge, -1, "Astrocyte-sleep bridge is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_sleep_get_effects: bridge is NULL");
    NIMCP_API_CHECK_NULL(effects, -1, "Effects output pointer is NULL");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_sleep_get_effects: effects is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->effects, sizeof(astrocyte_sleep_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float astrocyte_sleep_get_d_serine_level(const astrocyte_sleep_bridge_t bridge,
                                          float base_d_serine) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_sleep_get_d_serine_level: bridge is NULL");
        return base_d_serine;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->effects.d_serine_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return base_d_serine * factor;
}

float astrocyte_sleep_get_glutamate_uptake(const astrocyte_sleep_bridge_t bridge,
                                            float base_uptake) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_sleep_get_glutamate_uptake: bridge is NULL");
        return base_uptake;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->effects.glutamate_uptake_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return base_uptake * factor;
}

bool astrocyte_sleep_is_glymphatic_active(const astrocyte_sleep_bridge_t bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool active = bridge->effects.glymphatic_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    return active;
}
