/**
 * @file nimcp_cortical_hierarchy_sleep_bridge.c
 * @brief Sleep-Cortical Hierarchy Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "core/cortical_columns/sleep/nimcp_cortical_hierarchy_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_hierarchy_sleep_bridge)

struct cortical_hierarchy_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    cortical_hierarchy_sleep_config_t config;
    cortical_hierarchy_t* hierarchy;
    sleep_system_t sleep_system;
    cortical_hierarchy_sleep_effects_t effects;
    bool callback_registered;
};

static void cortical_hierarchy_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    cortical_hierarchy_sleep_bridge_t bridge = (cortical_hierarchy_sleep_bridge_t)user_data;
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->effects.current_state = new_state;

    if (bridge->config.enable_ff_modulation) {
        switch (new_state) {
            case SLEEP_STATE_LIGHT_NREM:
            case SLEEP_STATE_DEEP_NREM:
                bridge->effects.feedforward_strength = HIERARCHY_SLEEP_FF_NREM;
                break;
            case SLEEP_STATE_REM:
                bridge->effects.feedforward_strength = HIERARCHY_SLEEP_FF_REM;
                break;
            default:
                bridge->effects.feedforward_strength = HIERARCHY_SLEEP_FF_AWAKE;
                break;
        }
    }

    if (bridge->config.enable_fb_modulation) {
        switch (new_state) {
            case SLEEP_STATE_LIGHT_NREM:
            case SLEEP_STATE_DEEP_NREM:
                bridge->effects.feedback_strength = HIERARCHY_SLEEP_FB_NREM;
                break;
            case SLEEP_STATE_REM:
                bridge->effects.feedback_strength = HIERARCHY_SLEEP_FB_REM;
                break;
            default:
                bridge->effects.feedback_strength = HIERARCHY_SLEEP_FB_AWAKE;
                break;
        }
    }

    bridge->effects.hierarchy_offline = (new_state == SLEEP_STATE_DEEP_NREM);
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Cortical hierarchy modulated: FF=%.2f, FB=%.2f",
                        bridge->effects.feedforward_strength,
                        bridge->effects.feedback_strength);
}

int cortical_hierarchy_sleep_default_config(cortical_hierarchy_sleep_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_sleep_default_config: config is NULL");
        return -1;
    }
    config->enable_ff_modulation = true;
    config->enable_fb_modulation = true;
    config->modulation_strength = 1.0f;
    return 0;
}

cortical_hierarchy_sleep_bridge_t cortical_hierarchy_sleep_bridge_create(
    const cortical_hierarchy_sleep_config_t* config,
    cortical_hierarchy_t* hierarchy,
    sleep_system_t sleep)
{
    if (!hierarchy || !sleep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_sleep_bridge_create: required parameter is NULL (hierarchy, sleep)");
        return NULL;
    }

    struct cortical_hierarchy_sleep_bridge_struct* bridge =
        (struct cortical_hierarchy_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct cortical_hierarchy_sleep_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_hierarchy_sleep_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(struct cortical_hierarchy_sleep_bridge_struct));

    if (config) bridge->config = *config;
    else cortical_hierarchy_sleep_default_config(&bridge->config);

    bridge->hierarchy = hierarchy;
    bridge->sleep_system = sleep;

    if (bridge_base_init(&bridge->base, 0, "cortical_hierarchy_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.feedforward_strength = 1.0f;
    bridge->effects.feedback_strength = 1.0f;
    bridge->effects.hierarchy_offline = false;

    bool registered = sleep_register_state_callback(sleep, cortical_hierarchy_on_sleep_state_change, bridge);
    if (registered) {
        bridge->callback_registered = true;
        NIMCP_LOGGING_INFO("Cortical hierarchy sleep bridge created");
    }

    return bridge;
}

void cortical_hierarchy_sleep_bridge_destroy(cortical_hierarchy_sleep_bridge_t bridge)
{
    if (!bridge) return;
    if (bridge->callback_registered) {
        sleep_unregister_state_callback(bridge->sleep_system, cortical_hierarchy_on_sleep_state_change, bridge);
    }
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int cortical_hierarchy_sleep_update(cortical_hierarchy_sleep_bridge_t bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_sleep_update: bridge is NULL");
        return -1;
    }
    return 0;
}

int cortical_hierarchy_sleep_apply_modulation(cortical_hierarchy_sleep_bridge_t bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_sleep_apply_modulation: bridge is NULL");
        return -1;
    }
    /* Application would modify hierarchy FF/FB connection strengths */
    return 0;
}

int cortical_hierarchy_sleep_get_effects(const cortical_hierarchy_sleep_bridge_t bridge,
                                         cortical_hierarchy_sleep_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_hierarchy_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float cortical_hierarchy_sleep_get_ff_strength(const cortical_hierarchy_sleep_bridge_t bridge)
{
    if (!bridge) return -1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float strength = bridge->effects.feedforward_strength;
    nimcp_mutex_unlock(bridge->base.mutex);
    return strength;
}

float cortical_hierarchy_sleep_get_fb_strength(const cortical_hierarchy_sleep_bridge_t bridge)
{
    if (!bridge) return -1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float strength = bridge->effects.feedback_strength;
    nimcp_mutex_unlock(bridge->base.mutex);
    return strength;
}

bool cortical_hierarchy_sleep_is_offline(const cortical_hierarchy_sleep_bridge_t bridge)
{
    if (!bridge) {
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool offline = bridge->effects.hierarchy_offline;
    nimcp_mutex_unlock(bridge->base.mutex);
    return offline;
}
