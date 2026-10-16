/**
 * @file nimcp_pattern_db_sleep_bridge.c
 * @brief Sleep-Pattern Database Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Modulates pattern database behavior based on sleep state
 * WHY:  Pattern matching and consolidation should vary with consciousness
 * HOW:  Sleep state callbacks dynamically adjust pattern database parameters
 *
 * @author NIMCP Development Team
 */

#include "security/sleep/nimcp_pattern_db_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pattern_db_sleep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


struct pattern_db_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    pattern_db_sleep_config_t config;
    sleep_system_t sleep_system;
    pattern_db_sleep_effects_t effects;
    bool callback_registered;
};

float pattern_db_sleep_get_conf_factor(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return PATTERN_DB_SLEEP_CONF_AWAKE;
        case SLEEP_STATE_DROWSY:     return PATTERN_DB_SLEEP_CONF_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return PATTERN_DB_SLEEP_CONF_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return PATTERN_DB_SLEEP_CONF_DEEP_NREM;
        case SLEEP_STATE_REM:        return PATTERN_DB_SLEEP_CONF_REM;
        default:                     return PATTERN_DB_SLEEP_CONF_AWAKE;
    }
}

float pattern_db_sleep_get_prio_factor(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return PATTERN_DB_SLEEP_PRIO_AWAKE;
        case SLEEP_STATE_DROWSY:     return PATTERN_DB_SLEEP_PRIO_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return PATTERN_DB_SLEEP_PRIO_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return PATTERN_DB_SLEEP_PRIO_DEEP_NREM;
        case SLEEP_STATE_REM:        return PATTERN_DB_SLEEP_PRIO_REM;
        default:                     return PATTERN_DB_SLEEP_PRIO_AWAKE;
    }
}

static void pattern_db_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    pattern_db_sleep_bridge_t bridge = (pattern_db_sleep_bridge_t)user_data;
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;
    bridge->effects.confidence_factor = pattern_db_sleep_get_conf_factor(new_state);
    bridge->effects.priority_threshold_factor = pattern_db_sleep_get_prio_factor(new_state);

    /* Consolidation happens during sleep */
    bridge->effects.consolidating = (new_state == SLEEP_STATE_LIGHT_NREM ||
                                      new_state == SLEEP_STATE_DEEP_NREM ||
                                      new_state == SLEEP_STATE_REM);

    /* Updates only during awake or drowsy */
    bridge->effects.updates_allowed = (new_state == SLEEP_STATE_AWAKE ||
                                        new_state == SLEEP_STATE_DROWSY);

    NIMCP_LOGGING_DEBUG("Pattern DB sleep state changed to %d, conf=%.2f, prio=%.2f, consol=%d",
                        new_state, bridge->effects.confidence_factor,
                        bridge->effects.priority_threshold_factor,
                        bridge->effects.consolidating);

    nimcp_mutex_unlock(bridge->base.mutex);
}

int pattern_db_sleep_default_config(pattern_db_sleep_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_db_sleep_default_config: config is NULL");
        return -1;
    }

    config->enable_confidence_modulation = true;
    config->enable_priority_modulation = true;
    config->modulation_strength = 1.0f;

    return 0;
}

pattern_db_sleep_bridge_t pattern_db_sleep_bridge_create(
    const pattern_db_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    if (!config || !sleep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_db_sleep_bridge_create: config or sleep_system is NULL");
        NIMCP_LOGGING_ERROR("Invalid parameters for pattern DB sleep bridge creation");
        return NULL;
    }

    pattern_db_sleep_bridge_t bridge =
        (pattern_db_sleep_bridge_t)nimcp_malloc(sizeof(struct pattern_db_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_db_sleep_bridge_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate pattern DB sleep bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(struct pattern_db_sleep_bridge_struct));
    memcpy(&bridge->config, config, sizeof(pattern_db_sleep_config_t));
    bridge->sleep_system = sleep_system;

    if (bridge_base_init(&bridge->base, 0, "pattern_db_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pattern_db_sleep_bridge_create: mutex creation failed");
        NIMCP_LOGGING_ERROR("Failed to create mutex for pattern DB sleep bridge");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->effects.confidence_factor = PATTERN_DB_SLEEP_CONF_AWAKE;
    bridge->effects.priority_threshold_factor = PATTERN_DB_SLEEP_PRIO_AWAKE;
    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.sleep_pressure = 0.0f;
    bridge->effects.consolidating = false;
    bridge->effects.updates_allowed = true;

    bridge->callback_registered = sleep_register_state_callback(
        sleep_system, pattern_db_on_sleep_state_change, bridge);

    if (bridge->callback_registered) {
        sleep_state_t initial = sleep_get_current_state(sleep_system);
        pattern_db_on_sleep_state_change(initial, bridge);
    }

    NIMCP_LOGGING_INFO("Pattern DB sleep bridge created");
    return bridge;
}

void pattern_db_sleep_bridge_destroy(pattern_db_sleep_bridge_t bridge)
{
    if (!bridge) return;

    if (bridge->callback_registered && bridge->sleep_system) {
        sleep_unregister_state_callback(bridge->sleep_system,
                                        pattern_db_on_sleep_state_change, bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Pattern DB sleep bridge destroyed");
}

int pattern_db_sleep_update(pattern_db_sleep_bridge_t bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_db_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->effects.sleep_pressure = sleep_get_pressure(bridge->sleep_system);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pattern_db_sleep_get_effects(const pattern_db_sleep_bridge_t bridge,
                                  pattern_db_sleep_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_db_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->effects, sizeof(pattern_db_sleep_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float pattern_db_sleep_get_confidence_threshold(const pattern_db_sleep_bridge_t bridge, float base)
{
    if (!bridge) return base;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->config.enable_confidence_modulation ?
        bridge->effects.confidence_factor : 1.0f;
    float modulated = 1.0f + (factor - 1.0f) * bridge->config.modulation_strength;
    nimcp_mutex_unlock(bridge->base.mutex);

    return base * modulated;
}

float pattern_db_sleep_get_priority_threshold(const pattern_db_sleep_bridge_t bridge, float base)
{
    if (!bridge) return base;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->config.enable_priority_modulation ?
        bridge->effects.priority_threshold_factor : 1.0f;
    float modulated = 1.0f + (factor - 1.0f) * bridge->config.modulation_strength;
    nimcp_mutex_unlock(bridge->base.mutex);

    return base * modulated;
}

bool pattern_db_sleep_is_consolidating(const pattern_db_sleep_bridge_t bridge)
{
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool consolidating = bridge->effects.consolidating;
    nimcp_mutex_unlock(bridge->base.mutex);

    return consolidating;
}

bool pattern_db_sleep_allow_updates(const pattern_db_sleep_bridge_t bridge)
{
    if (!bridge) return true;

    nimcp_mutex_lock(bridge->base.mutex);
    bool allowed = bridge->effects.updates_allowed;
    nimcp_mutex_unlock(bridge->base.mutex);

    return allowed;
}
