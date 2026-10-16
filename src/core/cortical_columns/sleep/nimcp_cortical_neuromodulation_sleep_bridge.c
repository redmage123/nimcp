/**
 * @file nimcp_cortical_neuromodulation_sleep_bridge.c
 * @brief Sleep-Cortical Neuromodulation Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "core/cortical_columns/sleep/nimcp_cortical_neuromodulation_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_neuromodulation_sleep_bridge)

struct cortical_neuromodulation_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    cortical_neuromodulation_sleep_config_t config;
    void* neuromodulation_module;
    sleep_system_t sleep_system;
    cortical_neuromodulation_sleep_effects_t effects;
    bool callback_registered;
};

static void cortical_neuromodulation_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    cortical_neuromodulation_sleep_bridge_t bridge = (cortical_neuromodulation_sleep_bridge_t)user_data;
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->effects.current_state = new_state;

    /* Update neuromodulator levels based on sleep state */
    switch (new_state) {
        case SLEEP_STATE_AWAKE:
        case SLEEP_STATE_DROWSY:
            if (bridge->config.enable_ach_modulation)
                bridge->effects.acetylcholine_level = NEUROMOD_SLEEP_ACH_AWAKE;
            if (bridge->config.enable_ne_modulation)
                bridge->effects.norepinephrine_level = NEUROMOD_SLEEP_NE_AWAKE;
            if (bridge->config.enable_serotonin_modulation)
                bridge->effects.serotonin_level = NEUROMOD_SLEEP_SEROTONIN_AWAKE;
            break;

        case SLEEP_STATE_LIGHT_NREM:
        case SLEEP_STATE_DEEP_NREM:
            if (bridge->config.enable_ach_modulation)
                bridge->effects.acetylcholine_level = NEUROMOD_SLEEP_ACH_NREM;
            if (bridge->config.enable_ne_modulation)
                bridge->effects.norepinephrine_level = NEUROMOD_SLEEP_NE_NREM;
            if (bridge->config.enable_serotonin_modulation)
                bridge->effects.serotonin_level = NEUROMOD_SLEEP_SEROTONIN_NREM;
            break;

        case SLEEP_STATE_REM:
            /* REM paradox: High ACh, low NE/5-HT */
            if (bridge->config.enable_ach_modulation)
                bridge->effects.acetylcholine_level = NEUROMOD_SLEEP_ACH_REM;
            if (bridge->config.enable_ne_modulation)
                bridge->effects.norepinephrine_level = NEUROMOD_SLEEP_NE_REM;
            if (bridge->config.enable_serotonin_modulation)
                bridge->effects.serotonin_level = NEUROMOD_SLEEP_SEROTONIN_REM;
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Cortical neuromodulation updated: ACh=%.2f, NE=%.2f, 5-HT=%.2f",
                        bridge->effects.acetylcholine_level,
                        bridge->effects.norepinephrine_level,
                        bridge->effects.serotonin_level);
}

int cortical_neuromodulation_sleep_default_config(cortical_neuromodulation_sleep_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_neuromodulation_sleep_default_config: config is NULL");
        return -1;
    }
    config->enable_ach_modulation = true;
    config->enable_ne_modulation = true;
    config->enable_serotonin_modulation = true;
    config->modulation_strength = 1.0f;
    return 0;
}

cortical_neuromodulation_sleep_bridge_t cortical_neuromodulation_sleep_bridge_create(
    const cortical_neuromodulation_sleep_config_t* config,
    void* neuromodulation_module,
    sleep_system_t sleep)
{
    if (!neuromodulation_module || !sleep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_neuromodulation_sleep_bridge_create: required parameter is NULL (neuromodulation_module, sleep)");
        return NULL;
    }

    struct cortical_neuromodulation_sleep_bridge_struct* bridge =
        (struct cortical_neuromodulation_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct cortical_neuromodulation_sleep_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_neuromodulation_sleep_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(struct cortical_neuromodulation_sleep_bridge_struct));

    if (config) bridge->config = *config;
    else cortical_neuromodulation_sleep_default_config(&bridge->config);

    bridge->neuromodulation_module = neuromodulation_module;
    bridge->sleep_system = sleep;

    if (bridge_base_init(&bridge->base, 0, "cortical_neuromodulation_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_neuromodulation_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.acetylcholine_level = 1.0f;
    bridge->effects.norepinephrine_level = 1.0f;
    bridge->effects.serotonin_level = 1.0f;

    bool registered = sleep_register_state_callback(sleep, cortical_neuromodulation_on_sleep_state_change, bridge);
    if (registered) {
        bridge->callback_registered = true;
        NIMCP_LOGGING_INFO("Cortical neuromodulation sleep bridge created");
    }

    return bridge;
}

void cortical_neuromodulation_sleep_bridge_destroy(cortical_neuromodulation_sleep_bridge_t bridge)
{
    if (!bridge) return;
    if (bridge->callback_registered) {
        sleep_unregister_state_callback(bridge->sleep_system, cortical_neuromodulation_on_sleep_state_change, bridge);
    }
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int cortical_neuromodulation_sleep_update(cortical_neuromodulation_sleep_bridge_t bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_neuromodulation_sleep_update: bridge is NULL");
        return -1;
    }
    return 0;
}

float cortical_neuromodulation_sleep_get_ach(const cortical_neuromodulation_sleep_bridge_t bridge)
{
    if (!bridge) return -1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float ach = bridge->effects.acetylcholine_level;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ach;
}

float cortical_neuromodulation_sleep_get_ne(const cortical_neuromodulation_sleep_bridge_t bridge)
{
    if (!bridge) return -1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float ne = bridge->effects.norepinephrine_level;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ne;
}

float cortical_neuromodulation_sleep_get_serotonin(const cortical_neuromodulation_sleep_bridge_t bridge)
{
    if (!bridge) return -1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float serotonin = bridge->effects.serotonin_level;
    nimcp_mutex_unlock(bridge->base.mutex);
    return serotonin;
}
