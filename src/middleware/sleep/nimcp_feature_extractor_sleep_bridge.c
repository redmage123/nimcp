/**
 * @file nimcp_feature_extractor_sleep_bridge.c
 * @brief Sleep-Feature Extractor Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "middleware/sleep/nimcp_feature_extractor_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(feature_extractor_sleep_bridge)

struct feature_extractor_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    feature_extractor_sleep_config_t config;
    sleep_system_t sleep_system;
    feature_extractor_sleep_effects_t effects;
    bool callback_registered;
};

/* Forward declarations */
static void feature_extractor_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update feature extractor parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sensory processing degrades during sleep
 * - Detection thresholds rise as arousal decreases
 * - Deep NREM shows minimal sensory responsiveness
 * - REM enables selective, emotionally-biased extraction
 */
static void feature_extractor_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    feature_extractor_sleep_bridge_t bridge = (feature_extractor_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Feature extractor bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_threshold_modulation) {
        bridge->effects.detection_threshold =
            feature_extractor_sleep_get_threshold_factor(new_state);
    }

    if (bridge->config.enable_sensitivity_modulation) {
        float sensitivity_base = feature_extractor_sleep_get_sensitivity_factor(new_state);
        bridge->effects.extraction_sensitivity_factor =
            1.0f + (sensitivity_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_window_modulation) {
        float window_base = feature_extractor_sleep_get_window_factor(new_state);
        bridge->effects.window_duration_factor =
            1.0f + (window_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.extraction_enabled = (new_state != SLEEP_STATE_DEEP_NREM) ||
                                          bridge->effects.extraction_sensitivity_factor > 0.15f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Feature extractor modulated: threshold=%.2f, sensitivity=%.2f, window=%.2f",
                        bridge->effects.detection_threshold,
                        bridge->effects.extraction_sensitivity_factor,
                        bridge->effects.window_duration_factor);
}

int feature_extractor_sleep_default_config(feature_extractor_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_extractor_sleep_default_config: config is NULL");
        return -1;
    }
    config->enable_threshold_modulation = true;
    config->enable_sensitivity_modulation = true;
    config->enable_window_modulation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    return 0;
}

feature_extractor_sleep_bridge_t feature_extractor_sleep_bridge_create(
    const feature_extractor_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("feature_extractor_sleep_bridge_create: NULL sleep_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "feature_extractor_sleep_bridge_create: NULL sleep_system");
        return NULL;
    }

    struct feature_extractor_sleep_bridge_struct* bridge =
        (struct feature_extractor_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct feature_extractor_sleep_bridge_struct));
    NIMCP_API_CHECK_ALLOC_SIZE(bridge, sizeof(struct feature_extractor_sleep_bridge_struct),
        "feature_extractor_sleep_bridge_create: failed to allocate bridge");

    memset(bridge, 0, sizeof(struct feature_extractor_sleep_bridge_struct));

    if (config) {
        bridge->config = *config;
    } else {
        feature_extractor_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->effects.detection_threshold = 0.1f;
    bridge->effects.extraction_sensitivity_factor = 1.0f;
    bridge->effects.window_duration_factor = 1.0f;
    bridge->effects.extraction_enabled = true;

    if (bridge_base_init(&bridge->base, 0, "feature_extractor_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(nimcp_platform_mutex_t),
            "feature_extractor_sleep_bridge_create: failed to allocate mutex");
        nimcp_free(bridge);
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        feature_extractor_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for feature extractor bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    feature_extractor_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Feature extractor-sleep bridge created");
    return bridge;
}

void feature_extractor_sleep_bridge_destroy(feature_extractor_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            feature_extractor_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for feature extractor bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int feature_extractor_sleep_update(feature_extractor_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_extractor_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_threshold_modulation) {
        bridge->effects.detection_threshold =
            feature_extractor_sleep_get_threshold_factor(state);
    }

    if (bridge->config.enable_sensitivity_modulation) {
        float sensitivity_base = feature_extractor_sleep_get_sensitivity_factor(state);
        bridge->effects.extraction_sensitivity_factor =
            1.0f + (sensitivity_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_window_modulation) {
        float window_base = feature_extractor_sleep_get_window_factor(state);
        bridge->effects.window_duration_factor =
            1.0f + (window_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.extraction_enabled = (state != SLEEP_STATE_DEEP_NREM) ||
                                          bridge->effects.extraction_sensitivity_factor > 0.15f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int feature_extractor_sleep_get_effects(
    const feature_extractor_sleep_bridge_t bridge,
    feature_extractor_sleep_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "feature_extractor_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float feature_extractor_sleep_get_threshold(
    const feature_extractor_sleep_bridge_t bridge)
{
    if (!bridge) return 0.1f;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.detection_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float feature_extractor_sleep_get_sensitivity(
    const feature_extractor_sleep_bridge_t bridge)
{
    if (!bridge) return 1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.extraction_sensitivity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float feature_extractor_sleep_get_window_duration(
    const feature_extractor_sleep_bridge_t bridge)
{
    if (!bridge) return 1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.window_duration_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float feature_extractor_sleep_get_threshold_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return FEATURE_EXTRACTOR_SLEEP_THRESHOLD_AWAKE;
        case SLEEP_STATE_DROWSY:     return FEATURE_EXTRACTOR_SLEEP_THRESHOLD_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return FEATURE_EXTRACTOR_SLEEP_THRESHOLD_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return FEATURE_EXTRACTOR_SLEEP_THRESHOLD_DEEP_NREM;
        case SLEEP_STATE_REM:        return FEATURE_EXTRACTOR_SLEEP_THRESHOLD_REM;
        default:                     return FEATURE_EXTRACTOR_SLEEP_THRESHOLD_AWAKE;
    }
}

float feature_extractor_sleep_get_sensitivity_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return FEATURE_EXTRACTOR_SLEEP_SENSITIVITY_AWAKE;
        case SLEEP_STATE_DROWSY:     return FEATURE_EXTRACTOR_SLEEP_SENSITIVITY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return FEATURE_EXTRACTOR_SLEEP_SENSITIVITY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return FEATURE_EXTRACTOR_SLEEP_SENSITIVITY_DEEP_NREM;
        case SLEEP_STATE_REM:        return FEATURE_EXTRACTOR_SLEEP_SENSITIVITY_REM;
        default:                     return FEATURE_EXTRACTOR_SLEEP_SENSITIVITY_AWAKE;
    }
}

float feature_extractor_sleep_get_window_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return FEATURE_EXTRACTOR_SLEEP_WINDOW_AWAKE;
        case SLEEP_STATE_DROWSY:     return FEATURE_EXTRACTOR_SLEEP_WINDOW_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return FEATURE_EXTRACTOR_SLEEP_WINDOW_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return FEATURE_EXTRACTOR_SLEEP_WINDOW_DEEP_NREM;
        case SLEEP_STATE_REM:        return FEATURE_EXTRACTOR_SLEEP_WINDOW_REM;
        default:                     return FEATURE_EXTRACTOR_SLEEP_WINDOW_AWAKE;
    }
}
