/**
 * @file nimcp_anomaly_detector_sleep_bridge.c
 * @brief Sleep-Anomaly Detector Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Modulates anomaly detection based on sleep state
 * WHY:  Detection sensitivity and learning should vary with consciousness
 * HOW:  Sleep state callbacks dynamically adjust anomaly detection parameters
 *
 * @author NIMCP Development Team
 */

#include "security/sleep/nimcp_anomaly_detector_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(anomaly_detector_sleep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


struct anomaly_detector_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    anomaly_detector_sleep_config_t config;
    sleep_system_t sleep_system;
    anomaly_detector_sleep_effects_t effects;
    bool callback_registered;
};

float anomaly_sleep_get_thresh_factor(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ANOMALY_SLEEP_THRESH_AWAKE;
        case SLEEP_STATE_DROWSY:     return ANOMALY_SLEEP_THRESH_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ANOMALY_SLEEP_THRESH_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ANOMALY_SLEEP_THRESH_DEEP_NREM;
        case SLEEP_STATE_REM:        return ANOMALY_SLEEP_THRESH_REM;
        default:                     return ANOMALY_SLEEP_THRESH_AWAKE;
    }
}

float anomaly_sleep_get_learn_factor(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ANOMALY_SLEEP_LEARN_AWAKE;
        case SLEEP_STATE_DROWSY:     return ANOMALY_SLEEP_LEARN_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ANOMALY_SLEEP_LEARN_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ANOMALY_SLEEP_LEARN_DEEP_NREM;
        case SLEEP_STATE_REM:        return ANOMALY_SLEEP_LEARN_REM;
        default:                     return ANOMALY_SLEEP_LEARN_AWAKE;
    }
}

float anomaly_sleep_get_fp_factor(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ANOMALY_SLEEP_FP_AWAKE;
        case SLEEP_STATE_DROWSY:     return ANOMALY_SLEEP_FP_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ANOMALY_SLEEP_FP_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ANOMALY_SLEEP_FP_DEEP_NREM;
        case SLEEP_STATE_REM:        return ANOMALY_SLEEP_FP_REM;
        default:                     return ANOMALY_SLEEP_FP_AWAKE;
    }
}

static void anomaly_detector_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    anomaly_detector_sleep_bridge_t bridge = (anomaly_detector_sleep_bridge_t)user_data;
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;
    bridge->effects.anomaly_threshold_factor = anomaly_sleep_get_thresh_factor(new_state);
    bridge->effects.learning_rate_factor = anomaly_sleep_get_learn_factor(new_state);
    bridge->effects.fp_tolerance_factor = anomaly_sleep_get_fp_factor(new_state);
    bridge->effects.learning_enabled = (new_state == SLEEP_STATE_AWAKE ||
                                         new_state == SLEEP_STATE_DROWSY ||
                                         new_state == SLEEP_STATE_REM);

    NIMCP_LOGGING_DEBUG("Anomaly detector sleep state changed to %d, thresh=%.2f, learn=%.2f, fp=%.2f",
                        new_state, bridge->effects.anomaly_threshold_factor,
                        bridge->effects.learning_rate_factor,
                        bridge->effects.fp_tolerance_factor);

    nimcp_mutex_unlock(bridge->base.mutex);
}

int anomaly_detector_sleep_default_config(anomaly_detector_sleep_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "anomaly_detector_sleep_default_config: config is NULL");
        return -1;
    }

    config->enable_threshold_modulation = true;
    config->enable_learning_modulation = true;
    config->enable_fp_tolerance_modulation = true;
    config->modulation_strength = 1.0f;

    return 0;
}

anomaly_detector_sleep_bridge_t anomaly_detector_sleep_bridge_create(
    const anomaly_detector_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    if (!config || !sleep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "anomaly_detector_sleep_bridge_create: config or sleep_system is NULL");
        NIMCP_LOGGING_ERROR("Invalid parameters for anomaly detector sleep bridge creation");
        return NULL;
    }

    anomaly_detector_sleep_bridge_t bridge =
        (anomaly_detector_sleep_bridge_t)nimcp_malloc(sizeof(struct anomaly_detector_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "anomaly_detector_sleep_bridge_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate anomaly detector sleep bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(struct anomaly_detector_sleep_bridge_struct));
    memcpy(&bridge->config, config, sizeof(anomaly_detector_sleep_config_t));
    bridge->sleep_system = sleep_system;

    if (bridge_base_init(&bridge->base, 0, "anomaly_detector_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "anomaly_detector_sleep_bridge_create: mutex creation failed");
        NIMCP_LOGGING_ERROR("Failed to create mutex for anomaly detector sleep bridge");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->effects.anomaly_threshold_factor = ANOMALY_SLEEP_THRESH_AWAKE;
    bridge->effects.learning_rate_factor = ANOMALY_SLEEP_LEARN_AWAKE;
    bridge->effects.fp_tolerance_factor = ANOMALY_SLEEP_FP_AWAKE;
    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.sleep_pressure = 0.0f;
    bridge->effects.learning_enabled = true;

    bridge->callback_registered = sleep_register_state_callback(
        sleep_system, anomaly_detector_on_sleep_state_change, bridge);

    if (bridge->callback_registered) {
        sleep_state_t initial = sleep_get_current_state(sleep_system);
        anomaly_detector_on_sleep_state_change(initial, bridge);
    }

    NIMCP_LOGGING_INFO("Anomaly detector sleep bridge created");
    return bridge;
}

void anomaly_detector_sleep_bridge_destroy(anomaly_detector_sleep_bridge_t bridge)
{
    if (!bridge) return;

    if (bridge->callback_registered && bridge->sleep_system) {
        sleep_unregister_state_callback(bridge->sleep_system,
                                        anomaly_detector_on_sleep_state_change, bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Anomaly detector sleep bridge destroyed");
}

int anomaly_detector_sleep_update(anomaly_detector_sleep_bridge_t bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "anomaly_detector_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->effects.sleep_pressure = sleep_get_pressure(bridge->sleep_system);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int anomaly_detector_sleep_get_effects(const anomaly_detector_sleep_bridge_t bridge,
                                        anomaly_detector_sleep_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "anomaly_detector_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->effects, sizeof(anomaly_detector_sleep_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float anomaly_detector_sleep_get_threshold(const anomaly_detector_sleep_bridge_t bridge, float base)
{
    if (!bridge) return base;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->config.enable_threshold_modulation ?
        bridge->effects.anomaly_threshold_factor : 1.0f;
    float modulated = 1.0f + (factor - 1.0f) * bridge->config.modulation_strength;
    nimcp_mutex_unlock(bridge->base.mutex);

    return base * modulated;
}

float anomaly_detector_sleep_get_learning_rate(const anomaly_detector_sleep_bridge_t bridge, float base)
{
    if (!bridge) return base;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->config.enable_learning_modulation ?
        bridge->effects.learning_rate_factor : 1.0f;
    float modulated = 1.0f + (factor - 1.0f) * bridge->config.modulation_strength;
    nimcp_mutex_unlock(bridge->base.mutex);

    return base * modulated;
}

bool anomaly_detector_sleep_is_learning_enabled(const anomaly_detector_sleep_bridge_t bridge)
{
    if (!bridge) return true;

    nimcp_mutex_lock(bridge->base.mutex);
    bool enabled = bridge->effects.learning_enabled;
    nimcp_mutex_unlock(bridge->base.mutex);

    return enabled;
}
