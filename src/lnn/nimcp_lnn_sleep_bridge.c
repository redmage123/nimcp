/**
 * @file nimcp_lnn_sleep_bridge.c
 * @brief Sleep-LNN Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 */

#include "lnn/nimcp_lnn_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_sleep_bridge)

/**
 * @brief Internal structure for LNN sleep bridge
 *
 * WHAT: Complete state for sleep-LNN integration
 * WHY:  Encapsulate config, effects, sleep system handle
 * HOW:  Opaque pointer pattern for information hiding
 */
struct lnn_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    lnn_sleep_config_t config;          /**< Configuration */
    sleep_system_t sleep_system;        /**< Handle to sleep system */
    lnn_sleep_effects_t effects;        /**< Current modulation effects */
    nimcp_platform_mutex_t* mutex;      /**< Thread safety */
    bool callback_registered;           /**< Track callback for cleanup */

};

/* Forward declarations */
static void lnn_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * @brief Callback invoked when sleep state changes
 *
 * WHAT: Immediately update LNN dynamics parameters for new sleep state
 * WHY:  Respond instantly to sleep transitions
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state transitions alter neural oscillation frequencies
 * - Liquid dynamics should match these frequency changes
 * - Deep NREM (delta) requires slower integration than REM (theta)
 */
static void lnn_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    lnn_sleep_bridge_t bridge = (lnn_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("LNN bridge received sleep state: %d", new_state);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Update tau factor (time constant modulation) */
    if (bridge->config.enable_tau_modulation) {
        float tau_base = lnn_sleep_tau_for_state(new_state);
        bridge->effects.tau_factor =
            1.0f + (tau_base - 1.0f) * bridge->config.modulation_strength;
    }

    /* Update dt factor (ODE time step modulation) */
    if (bridge->config.enable_dt_modulation) {
        float dt_base = lnn_sleep_dt_for_state(new_state);
        bridge->effects.dt_factor =
            1.0f + (dt_base - 1.0f) * bridge->config.modulation_strength;
    }

    /* Update learning rate factor */
    if (bridge->config.enable_lr_modulation) {
        float lr_base = lnn_sleep_lr_for_state(new_state);
        bridge->effects.learning_rate_factor =
            1.0f + (lr_base - 1.0f) * bridge->config.modulation_strength;
    }

    /* Dynamics are slowed if tau or dt increased */
    bridge->effects.dynamics_slowed =
        (bridge->effects.tau_factor > 1.0f) || (bridge->effects.dt_factor > 1.0f);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("LNN modulated: tau=%.2f, dt=%.2f, lr=%.2f, slowed=%d",
                        bridge->effects.tau_factor,
                        bridge->effects.dt_factor,
                        bridge->effects.learning_rate_factor,
                        bridge->effects.dynamics_slowed);
}

int lnn_sleep_default_config(lnn_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_sleep_default_config: config is NULL");
        return -1;
    }

    /* Enable all modulations by default */
    config->enable_tau_modulation = true;
    config->enable_dt_modulation = true;
    config->enable_lr_modulation = true;
    config->modulation_strength = 1.0f;

    return 0;
}

lnn_sleep_bridge_t lnn_sleep_bridge_create(
    const lnn_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    if (!sleep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "NULL sleep_system in lnn_sleep_bridge_create");
        return NULL;
    }

    struct lnn_sleep_bridge_struct* bridge =
        (struct lnn_sleep_bridge_struct*)nimcp_malloc(sizeof(struct lnn_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_sleep_bridge_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(struct lnn_sleep_bridge_struct));

    /* Initialize configuration */
    if (config) {
        bridge->config = *config;
    } else {
        lnn_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;

    /* Initialize effects to neutral (no modulation) */
    bridge->effects.tau_factor = 1.0f;
    bridge->effects.dt_factor = 1.0f;
    bridge->effects.learning_rate_factor = 1.0f;
    bridge->effects.dynamics_slowed = false;

    /* Create mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "lnn_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        lnn_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for LNN bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    lnn_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("LNN-sleep bridge created");
    return bridge;
}

void lnn_sleep_bridge_destroy(lnn_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            lnn_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for LNN bridge");
        }
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        lnn_sleep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int lnn_sleep_update(lnn_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Query current sleep state and pressure */
    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    /* Recompute modulation factors */
    if (bridge->config.enable_tau_modulation) {
        float tau_base = lnn_sleep_tau_for_state(state);
        bridge->effects.tau_factor =
            1.0f + (tau_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_dt_modulation) {
        float dt_base = lnn_sleep_dt_for_state(state);
        bridge->effects.dt_factor =
            1.0f + (dt_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_lr_modulation) {
        float lr_base = lnn_sleep_lr_for_state(state);
        bridge->effects.learning_rate_factor =
            1.0f + (lr_base - 1.0f) * bridge->config.modulation_strength;
    }

    /* Update dynamics_slowed flag */
    bridge->effects.dynamics_slowed =
        (bridge->effects.tau_factor > 1.0f) || (bridge->effects.dt_factor > 1.0f);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int lnn_sleep_get_effects(const lnn_sleep_bridge_t bridge, lnn_sleep_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

float lnn_sleep_get_tau_factor(const lnn_sleep_bridge_t bridge) {
    if (!bridge) return 1.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.tau_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return result;
}

float lnn_sleep_get_dt_factor(const lnn_sleep_bridge_t bridge) {
    if (!bridge) return 1.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.dt_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return result;
}

float lnn_sleep_get_lr_factor(const lnn_sleep_bridge_t bridge) {
    if (!bridge) return 1.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.learning_rate_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return result;
}

float lnn_sleep_tau_for_state(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return LNN_SLEEP_TAU_AWAKE;
        case SLEEP_STATE_DROWSY:     return LNN_SLEEP_TAU_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return LNN_SLEEP_TAU_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return LNN_SLEEP_TAU_DEEP_NREM;
        case SLEEP_STATE_REM:        return LNN_SLEEP_TAU_REM;
        default:                     return LNN_SLEEP_TAU_AWAKE;
    }
}

float lnn_sleep_dt_for_state(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return LNN_SLEEP_DT_AWAKE;
        case SLEEP_STATE_DROWSY:     return LNN_SLEEP_DT_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return LNN_SLEEP_DT_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return LNN_SLEEP_DT_DEEP_NREM;
        case SLEEP_STATE_REM:        return LNN_SLEEP_DT_REM;
        default:                     return LNN_SLEEP_DT_AWAKE;
    }
}

float lnn_sleep_lr_for_state(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return LNN_SLEEP_LR_AWAKE;
        case SLEEP_STATE_DROWSY:     return LNN_SLEEP_LR_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return LNN_SLEEP_LR_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return LNN_SLEEP_LR_DEEP_NREM;
        case SLEEP_STATE_REM:        return LNN_SLEEP_LR_REM;
        default:                     return LNN_SLEEP_LR_AWAKE;
    }
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed LNN sleep signals
 * HOW:  Register with bio_router using BIO_MODULE_LNN_SLEEP
 */
int lnn_sleep_bridge_connect_bio_async(lnn_sleep_bridge_t bridge)
{
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_sleep_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;  /* Already connected */

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_LNN_SLEEP,
        .module_name = "lnn_sleep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("LNN-sleep bridge connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return 0;  /* Not an error if router unavailable */
}

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 */
int lnn_sleep_bridge_disconnect_bio_async(lnn_sleep_bridge_t bridge)
{
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_sleep_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;  /* Not connected */

    /* Unregister from bio-async router */
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("LNN-sleep bridge disconnected from bio-async router");

    return 0;
}

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Allow conditional bio-async usage
 * HOW:  Return bio_async_enabled flag
 */
bool lnn_sleep_bridge_is_bio_async_connected(const lnn_sleep_bridge_t bridge)
{
    /* Guard clause */
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}
