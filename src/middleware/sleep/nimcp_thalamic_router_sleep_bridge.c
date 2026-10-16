/**
 * @file nimcp_thalamic_router_sleep_bridge.c
 * @brief Sleep-Thalamic Router Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "middleware/sleep/nimcp_thalamic_router_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(thalamic_router_sleep_bridge)

struct thalamic_router_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    thalamic_router_sleep_config_t config;
    sleep_system_t sleep_system;
    thalamic_router_sleep_effects_t effects;
    bool callback_registered;
};

/* Forward declarations */
static void thalamic_router_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update thalamic router parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Thalamic relay mode shifts from tonic (awake) to burst (NREM) firing
 * - TRN generates spindles during NREM, reducing cortical communication
 * - Deep NREM silences thalamus, preventing sensory relay
 * - REM restores relay but with altered selectivity
 */
static void thalamic_router_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    thalamic_router_sleep_bridge_t bridge = (thalamic_router_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Thalamic router bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_routing_modulation) {
        float routing_base = thalamic_router_sleep_get_routing_factor(new_state);
        bridge->effects.routing_efficiency_factor =
            1.0f + (routing_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_queue_modulation) {
        float queue_base = thalamic_router_sleep_get_queue_factor(new_state);
        bridge->effects.queue_capacity_factor =
            1.0f + (queue_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_attention_modulation) {
        float attention_base = thalamic_router_sleep_get_attention_factor(new_state);
        bridge->effects.attention_threshold = attention_base;
    }

    bridge->effects.routing_enabled = (new_state != SLEEP_STATE_DEEP_NREM) ||
                                       bridge->effects.routing_efficiency_factor > 0.15f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Thalamic router modulated: routing=%.2f, queue=%.2f, attention=%.2f",
                        bridge->effects.routing_efficiency_factor,
                        bridge->effects.queue_capacity_factor,
                        bridge->effects.attention_threshold);
}

int thalamic_router_sleep_default_config(thalamic_router_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_sleep_default_config: config is NULL");
        return -1;
    }
    config->enable_routing_modulation = true;
    config->enable_queue_modulation = true;
    config->enable_attention_modulation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    return 0;
}

thalamic_router_sleep_bridge_t thalamic_router_sleep_bridge_create(
    const thalamic_router_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("thalamic_router_sleep_bridge_create: NULL sleep_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "thalamic_router_sleep_bridge_create: NULL sleep_system");
        return NULL;
    }

    struct thalamic_router_sleep_bridge_struct* bridge =
        (struct thalamic_router_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct thalamic_router_sleep_bridge_struct));
    NIMCP_API_CHECK_ALLOC_SIZE(bridge, sizeof(struct thalamic_router_sleep_bridge_struct),
        "thalamic_router_sleep_bridge_create: failed to allocate bridge");

    memset(bridge, 0, sizeof(struct thalamic_router_sleep_bridge_struct));

    if (config) {
        bridge->config = *config;
    } else {
        thalamic_router_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->effects.routing_efficiency_factor = 1.0f;
    bridge->effects.queue_capacity_factor = 1.0f;
    bridge->effects.attention_threshold = 0.3f;
    bridge->effects.routing_enabled = true;

    if (bridge_base_init(&bridge->base, 0, "thalamic_router_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(nimcp_platform_mutex_t),
            "thalamic_router_sleep_bridge_create: failed to allocate mutex");
        nimcp_free(bridge);
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        thalamic_router_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for thalamic router bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    thalamic_router_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Thalamic router-sleep bridge created");
    return bridge;
}

void thalamic_router_sleep_bridge_destroy(thalamic_router_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            thalamic_router_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for thalamic router bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int thalamic_router_sleep_update(thalamic_router_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_routing_modulation) {
        float routing_base = thalamic_router_sleep_get_routing_factor(state);
        bridge->effects.routing_efficiency_factor =
            1.0f + (routing_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_queue_modulation) {
        float queue_base = thalamic_router_sleep_get_queue_factor(state);
        bridge->effects.queue_capacity_factor =
            1.0f + (queue_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_attention_modulation) {
        float attention_base = thalamic_router_sleep_get_attention_factor(state);
        bridge->effects.attention_threshold = attention_base;
    }

    bridge->effects.routing_enabled = (state != SLEEP_STATE_DEEP_NREM) ||
                                       bridge->effects.routing_efficiency_factor > 0.15f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int thalamic_router_sleep_get_effects(
    const thalamic_router_sleep_bridge_t bridge,
    thalamic_router_sleep_effects_t* effects)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_sleep_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_sleep_get_effects: effects is NULL");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float thalamic_router_sleep_get_routing_efficiency(
    const thalamic_router_sleep_bridge_t bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_sleep_get_routing_efficiency: bridge is NULL");
        return 1.0f;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.routing_efficiency_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float thalamic_router_sleep_get_queue_capacity(
    const thalamic_router_sleep_bridge_t bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_sleep_get_queue_capacity: bridge is NULL");
        return 1.0f;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.queue_capacity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float thalamic_router_sleep_get_attention_threshold(
    const thalamic_router_sleep_bridge_t bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thalamic_router_sleep_get_attention_threshold: bridge is NULL");
        return 0.3f;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.attention_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float thalamic_router_sleep_get_routing_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return THALAMIC_SLEEP_ROUTING_AWAKE;
        case SLEEP_STATE_DROWSY:     return THALAMIC_SLEEP_ROUTING_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return THALAMIC_SLEEP_ROUTING_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return THALAMIC_SLEEP_ROUTING_DEEP_NREM;
        case SLEEP_STATE_REM:        return THALAMIC_SLEEP_ROUTING_REM;
        default:                     return THALAMIC_SLEEP_ROUTING_AWAKE;
    }
}

float thalamic_router_sleep_get_queue_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return THALAMIC_SLEEP_QUEUE_AWAKE;
        case SLEEP_STATE_DROWSY:     return THALAMIC_SLEEP_QUEUE_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return THALAMIC_SLEEP_QUEUE_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return THALAMIC_SLEEP_QUEUE_DEEP_NREM;
        case SLEEP_STATE_REM:        return THALAMIC_SLEEP_QUEUE_REM;
        default:                     return THALAMIC_SLEEP_QUEUE_AWAKE;
    }
}

float thalamic_router_sleep_get_attention_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return THALAMIC_SLEEP_ATTENTION_AWAKE;
        case SLEEP_STATE_DROWSY:     return THALAMIC_SLEEP_ATTENTION_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return THALAMIC_SLEEP_ATTENTION_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return THALAMIC_SLEEP_ATTENTION_DEEP_NREM;
        case SLEEP_STATE_REM:        return THALAMIC_SLEEP_ATTENTION_REM;
        default:                     return THALAMIC_SLEEP_ATTENTION_AWAKE;
    }
}
