/**
 * @file nimcp_cortical_temporal_sleep_bridge.c
 * @brief Sleep-Cortical Temporal Processing Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "core/cortical_columns/sleep/nimcp_cortical_temporal_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_temporal_sleep_bridge)

#define LOG_MODULE "CORTICAL_TEMPORAL_SLEEP_BRIDGE"


struct cortical_temporal_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    cortical_temporal_sleep_config_t config;
    void* temporal_module;
    sleep_system_t sleep_system;
    cortical_temporal_sleep_effects_t effects;
    bool callback_registered;
};

static void cortical_temporal_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    cortical_temporal_sleep_bridge_t bridge = (cortical_temporal_sleep_bridge_t)user_data;
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->effects.current_state = new_state;

    if (bridge->config.enable_timescale_modulation) {
        switch (new_state) {
            case SLEEP_STATE_AWAKE:
            case SLEEP_STATE_DROWSY:
                bridge->effects.timescale_factor = TEMPORAL_SLEEP_TIMESCALE_AWAKE;
                break;
            case SLEEP_STATE_LIGHT_NREM:
            case SLEEP_STATE_DEEP_NREM:
                bridge->effects.timescale_factor = TEMPORAL_SLEEP_TIMESCALE_NREM;
                break;
            case SLEEP_STATE_REM:
                bridge->effects.timescale_factor = TEMPORAL_SLEEP_TIMESCALE_REM;
                break;
        }
    }

    bridge->effects.temporal_offline = (new_state == SLEEP_STATE_DEEP_NREM);
    nimcp_mutex_unlock(bridge->base.mutex);
}

int cortical_temporal_sleep_default_config(cortical_temporal_sleep_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_temporal_sleep_default_config: config is NULL");
        return -1;
    }
    config->enable_timescale_modulation = true;
    config->modulation_strength = 1.0f;
    return 0;
}

cortical_temporal_sleep_bridge_t cortical_temporal_sleep_bridge_create(
    const cortical_temporal_sleep_config_t* config,
    void* temporal_module,
    sleep_system_t sleep)
{
    if (!temporal_module || !sleep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_temporal_sleep_bridge_create: required parameter is NULL (temporal_module, sleep)");
        return NULL;
    }

    struct cortical_temporal_sleep_bridge_struct* bridge =
        (struct cortical_temporal_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct cortical_temporal_sleep_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_temporal_sleep_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(struct cortical_temporal_sleep_bridge_struct));

    if (config) bridge->config = *config;
    else cortical_temporal_sleep_default_config(&bridge->config);

    bridge->temporal_module = temporal_module;
    bridge->sleep_system = sleep;

    if (bridge_base_init(&bridge->base, 0, "cortical_temporal_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_temporal_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.timescale_factor = 1.0f;
    bridge->effects.temporal_offline = false;

    bool registered = sleep_register_state_callback(sleep, cortical_temporal_on_sleep_state_change, bridge);
    if (registered) bridge->callback_registered = true;

    return bridge;
}

void cortical_temporal_sleep_bridge_destroy(cortical_temporal_sleep_bridge_t bridge)
{
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cortical_temporal_sleep");
    if (bridge->callback_registered) {
        sleep_unregister_state_callback(bridge->sleep_system, cortical_temporal_on_sleep_state_change, bridge);
    }
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int cortical_temporal_sleep_update(cortical_temporal_sleep_bridge_t bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_temporal_sleep_update: bridge is NULL");
        return -1;
    }
    return 0;
}

float cortical_temporal_sleep_get_timescale(const cortical_temporal_sleep_bridge_t bridge)
{
    if (!bridge) return -1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float timescale = bridge->effects.timescale_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return timescale;
}
