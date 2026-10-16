/**
 * @file nimcp_sequence_detector_sleep_bridge.c
 * @brief Sleep-Sequence Detector Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "middleware/sleep/nimcp_sequence_detector_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sequence_detector_sleep_bridge)

struct sequence_detector_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    sequence_detector_sleep_config_t config;
    sleep_system_t sleep_system;
    sequence_detector_sleep_effects_t effects;
    bool callback_registered;
};

/* Forward declarations */
static void sequence_detector_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update sequence detector parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal replay is enhanced during NREM sleep
 * - Sequence detection benefits from relaxed temporal constraints in sleep
 * - Deep NREM shows maximum replay activity (slow wave-coupled)
 * - REM enables creative sequence recombination
 */
static void sequence_detector_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    sequence_detector_sleep_bridge_t bridge = (sequence_detector_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Sequence detector bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_sensitivity_modulation) {
        float sensitivity_base = sequence_detector_sleep_get_sensitivity_factor(new_state);
        bridge->effects.matching_sensitivity_factor =
            1.0f + (sensitivity_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_tolerance_modulation) {
        float tolerance_base = sequence_detector_sleep_get_tolerance_factor(new_state);
        bridge->effects.temporal_tolerance_factor =
            1.0f + (tolerance_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_threshold_modulation) {
        bridge->effects.min_strength_threshold =
            sequence_detector_sleep_get_min_strength_factor(new_state);
    }

    if (bridge->config.enable_replay_enhancement) {
        bridge->effects.replay_detection_enhanced =
            sequence_detector_sleep_is_replay_state(new_state);
    }

    /* Sequence detection enabled in all sleep states (especially NREM for replay) */
    bridge->effects.detection_enabled = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Sequence detector modulated: sensitivity=%.2f, tolerance=%.2f, min_strength=%.2f, replay=%d",
                        bridge->effects.matching_sensitivity_factor,
                        bridge->effects.temporal_tolerance_factor,
                        bridge->effects.min_strength_threshold,
                        bridge->effects.replay_detection_enhanced);
}

int sequence_detector_sleep_default_config(sequence_detector_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_sleep_default_config: config is NULL");
        return -1;
    }
    config->enable_sensitivity_modulation = true;
    config->enable_tolerance_modulation = true;
    config->enable_threshold_modulation = true;
    config->enable_replay_enhancement = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    return 0;
}

sequence_detector_sleep_bridge_t sequence_detector_sleep_bridge_create(
    const sequence_detector_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("sequence_detector_sleep_bridge_create: NULL sleep_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "sequence_detector_sleep_bridge_create: NULL sleep_system");
        return NULL;
    }

    struct sequence_detector_sleep_bridge_struct* bridge =
        (struct sequence_detector_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct sequence_detector_sleep_bridge_struct));
    NIMCP_API_CHECK_ALLOC_SIZE(bridge, sizeof(struct sequence_detector_sleep_bridge_struct),
        "sequence_detector_sleep_bridge_create: failed to allocate bridge");

    memset(bridge, 0, sizeof(struct sequence_detector_sleep_bridge_struct));

    if (config) {
        bridge->config = *config;
    } else {
        sequence_detector_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->effects.matching_sensitivity_factor = 1.0f;
    bridge->effects.temporal_tolerance_factor = 1.0f;
    bridge->effects.min_strength_threshold = 0.5f;
    bridge->effects.replay_detection_enhanced = false;
    bridge->effects.detection_enabled = true;

    if (bridge_base_init(&bridge->base, 0, "sequence_detector_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(nimcp_platform_mutex_t),
            "sequence_detector_sleep_bridge_create: failed to allocate mutex");
        nimcp_free(bridge);
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        sequence_detector_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for sequence detector bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    sequence_detector_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Sequence detector-sleep bridge created");
    return bridge;
}

void sequence_detector_sleep_bridge_destroy(sequence_detector_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            sequence_detector_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for sequence detector bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int sequence_detector_sleep_update(sequence_detector_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_sensitivity_modulation) {
        float sensitivity_base = sequence_detector_sleep_get_sensitivity_factor(state);
        bridge->effects.matching_sensitivity_factor =
            1.0f + (sensitivity_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_tolerance_modulation) {
        float tolerance_base = sequence_detector_sleep_get_tolerance_factor(state);
        bridge->effects.temporal_tolerance_factor =
            1.0f + (tolerance_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_threshold_modulation) {
        bridge->effects.min_strength_threshold =
            sequence_detector_sleep_get_min_strength_factor(state);
    }

    if (bridge->config.enable_replay_enhancement) {
        bridge->effects.replay_detection_enhanced =
            sequence_detector_sleep_is_replay_state(state);
    }

    bridge->effects.detection_enabled = true;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sequence_detector_sleep_get_effects(
    const sequence_detector_sleep_bridge_t bridge,
    sequence_detector_sleep_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sequence_detector_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float sequence_detector_sleep_get_sensitivity(
    const sequence_detector_sleep_bridge_t bridge)
{
    if (!bridge) return 1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.matching_sensitivity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float sequence_detector_sleep_get_tolerance(
    const sequence_detector_sleep_bridge_t bridge)
{
    if (!bridge) return 1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.temporal_tolerance_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float sequence_detector_sleep_get_min_strength(
    const sequence_detector_sleep_bridge_t bridge)
{
    if (!bridge) return 0.5f;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.min_strength_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

bool sequence_detector_sleep_is_replay_enhanced(
    const sequence_detector_sleep_bridge_t bridge)
{
    if (!bridge) {
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.replay_detection_enhanced;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float sequence_detector_sleep_get_sensitivity_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return SEQUENCE_DETECTOR_SLEEP_SENSITIVITY_AWAKE;
        case SLEEP_STATE_DROWSY:     return SEQUENCE_DETECTOR_SLEEP_SENSITIVITY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return SEQUENCE_DETECTOR_SLEEP_SENSITIVITY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return SEQUENCE_DETECTOR_SLEEP_SENSITIVITY_DEEP_NREM;
        case SLEEP_STATE_REM:        return SEQUENCE_DETECTOR_SLEEP_SENSITIVITY_REM;
        default:                     return SEQUENCE_DETECTOR_SLEEP_SENSITIVITY_AWAKE;
    }
}

float sequence_detector_sleep_get_tolerance_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return SEQUENCE_DETECTOR_SLEEP_TOLERANCE_AWAKE;
        case SLEEP_STATE_DROWSY:     return SEQUENCE_DETECTOR_SLEEP_TOLERANCE_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return SEQUENCE_DETECTOR_SLEEP_TOLERANCE_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return SEQUENCE_DETECTOR_SLEEP_TOLERANCE_DEEP_NREM;
        case SLEEP_STATE_REM:        return SEQUENCE_DETECTOR_SLEEP_TOLERANCE_REM;
        default:                     return SEQUENCE_DETECTOR_SLEEP_TOLERANCE_AWAKE;
    }
}

float sequence_detector_sleep_get_min_strength_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return SEQUENCE_DETECTOR_SLEEP_MIN_STRENGTH_AWAKE;
        case SLEEP_STATE_DROWSY:     return SEQUENCE_DETECTOR_SLEEP_MIN_STRENGTH_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return SEQUENCE_DETECTOR_SLEEP_MIN_STRENGTH_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return SEQUENCE_DETECTOR_SLEEP_MIN_STRENGTH_DEEP_NREM;
        case SLEEP_STATE_REM:        return SEQUENCE_DETECTOR_SLEEP_MIN_STRENGTH_REM;
        default:                     return SEQUENCE_DETECTOR_SLEEP_MIN_STRENGTH_AWAKE;
    }
}

bool sequence_detector_sleep_is_replay_state(sleep_state_t state) {
    /* Replay is enhanced in NREM states */
    return (state == SLEEP_STATE_LIGHT_NREM || state == SLEEP_STATE_DEEP_NREM);
}
