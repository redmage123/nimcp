/**
 * @file nimcp_visual_cortex_sleep_bridge.c
 * @brief Sleep-Visual Cortex Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "perception/sleep/nimcp_visual_cortex_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(visual_cortex_sleep_bridge)

struct visual_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    visual_sleep_config_t config;
    sleep_system_t sleep_system;
    visual_sleep_effects_t effects;
    bool callback_registered;  /* Track if callback is registered for cleanup */
};

/* Forward declarations */
static void visual_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update visual cortex parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Visual acuity degrades with sleep pressure (microsleeps, fatigue)
 * - Contrast sensitivity drops during drowsiness (traffic safety research)
 * - Thalamic gating blocks visual input during NREM sleep
 * - REM sleep has internal visual imagery but suppressed external processing
 */
static void visual_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    visual_sleep_bridge_t bridge = (visual_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Visual cortex bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_acuity_modulation) {
        float acuity_base = visual_sleep_get_acuity_factor(new_state);
        bridge->effects.acuity_factor =
            1.0f + (acuity_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_contrast_modulation) {
        float contrast_base = visual_sleep_get_contrast_factor(new_state);
        bridge->effects.contrast_sensitivity_factor =
            1.0f + (contrast_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_attention_modulation) {
        float attention_base = visual_sleep_get_attention_factor(new_state);
        bridge->effects.attention_gate =
            1.0f + (attention_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.visual_processing_enabled = (new_state != SLEEP_STATE_DEEP_NREM) ||
                                                  bridge->effects.acuity_factor > 0.2f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Visual modulated: acuity=%.2f, contrast=%.2f, attention=%.2f",
                        bridge->effects.acuity_factor,
                        bridge->effects.contrast_sensitivity_factor,
                        bridge->effects.attention_gate);
}

int visual_sleep_default_config(visual_sleep_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "visual_sleep_default_config: NULL config");
    config->enable_acuity_modulation = true;
    config->enable_contrast_modulation = true;
    config->enable_attention_modulation = true;
    config->modulation_strength = 1.0f;
    return 0;
}

visual_sleep_bridge_t visual_sleep_bridge_create(
    const visual_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    NIMCP_API_CHECK_NULL_RET_NULL(sleep_system, "visual_sleep_bridge_create: NULL sleep_system");

    struct visual_sleep_bridge_struct* bridge =
        (struct visual_sleep_bridge_struct*)nimcp_malloc(sizeof(struct visual_sleep_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "visual_sleep_bridge_create: Failed to allocate bridge");

    memset(bridge, 0, sizeof(struct visual_sleep_bridge_struct));

    if (config) {
        bridge->config = *config;
    } else {
        visual_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->effects.acuity_factor = 1.0f;
    bridge->effects.contrast_sensitivity_factor = 1.0f;
    bridge->effects.attention_gate = 1.0f;
    bridge->effects.visual_processing_enabled = true;

    if (bridge_base_init(&bridge->base, 0, "visual_cortex_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        visual_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for visual cortex bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    visual_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Visual cortex-sleep bridge created");
    return bridge;
}

void visual_sleep_bridge_destroy(visual_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            visual_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for visual cortex bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int visual_sleep_update(visual_sleep_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_sleep_update: NULL bridge");

    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_acuity_modulation) {
        float acuity_base = visual_sleep_get_acuity_factor(state);
        bridge->effects.acuity_factor =
            1.0f + (acuity_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_contrast_modulation) {
        float contrast_base = visual_sleep_get_contrast_factor(state);
        bridge->effects.contrast_sensitivity_factor =
            1.0f + (contrast_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_attention_modulation) {
        float attention_base = visual_sleep_get_attention_factor(state);
        bridge->effects.attention_gate =
            1.0f + (attention_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.visual_processing_enabled = (state != SLEEP_STATE_DEEP_NREM) ||
                                                  bridge->effects.acuity_factor > 0.2f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int visual_sleep_get_effects(const visual_sleep_bridge_t bridge, visual_sleep_effects_t* effects) {
    NIMCP_API_CHECK_NULL(bridge, -1, "visual_sleep_get_effects: NULL bridge");
    NIMCP_API_CHECK_NULL(effects, -1, "visual_sleep_get_effects: NULL effects");
    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float visual_sleep_get_acuity(const visual_sleep_bridge_t bridge, float base_acuity) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_sleep_get_acuity: bridge is NULL");
        return base_acuity;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    float result = base_acuity * bridge->effects.acuity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float visual_sleep_get_contrast_sensitivity(const visual_sleep_bridge_t bridge, float base_contrast) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_sleep_get_contrast_sensitivity: bridge is NULL");
        return base_contrast;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    float result = base_contrast * bridge->effects.contrast_sensitivity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float visual_sleep_get_attention_gate(const visual_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_sleep_get_attention_gate: bridge is NULL");
        return 1.0f;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.attention_gate;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float visual_sleep_get_acuity_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return VISUAL_SLEEP_ACUITY_AWAKE;
        case SLEEP_STATE_DROWSY:     return VISUAL_SLEEP_ACUITY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return VISUAL_SLEEP_ACUITY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return VISUAL_SLEEP_ACUITY_DEEP_NREM;
        case SLEEP_STATE_REM:        return VISUAL_SLEEP_ACUITY_REM;
        default:                     return VISUAL_SLEEP_ACUITY_AWAKE;
    }
}

float visual_sleep_get_contrast_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return VISUAL_SLEEP_CONTRAST_AWAKE;
        case SLEEP_STATE_DROWSY:     return VISUAL_SLEEP_CONTRAST_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return VISUAL_SLEEP_CONTRAST_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return VISUAL_SLEEP_CONTRAST_DEEP_NREM;
        case SLEEP_STATE_REM:        return VISUAL_SLEEP_CONTRAST_REM;
        default:                     return VISUAL_SLEEP_CONTRAST_AWAKE;
    }
}

float visual_sleep_get_attention_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return VISUAL_SLEEP_ATTENTION_AWAKE;
        case SLEEP_STATE_DROWSY:     return VISUAL_SLEEP_ATTENTION_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return VISUAL_SLEEP_ATTENTION_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return VISUAL_SLEEP_ATTENTION_DEEP_NREM;
        case SLEEP_STATE_REM:        return VISUAL_SLEEP_ATTENTION_REM;
        default:                     return VISUAL_SLEEP_ATTENTION_AWAKE;
    }
}
