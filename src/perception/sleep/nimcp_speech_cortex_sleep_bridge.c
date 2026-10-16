/**
 * @file nimcp_speech_cortex_sleep_bridge.c
 * @brief Sleep-Speech Cortex Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "perception/sleep/nimcp_speech_cortex_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(speech_cortex_sleep_bridge)

struct speech_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    speech_sleep_config_t config;
    sleep_system_t sleep_system;
    speech_sleep_effects_t effects;
    bool callback_registered;  /* Track if callback is registered for cleanup */
};

/* Forward declarations */
static void speech_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update speech cortex parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Phoneme discrimination requires attentional resources (reduced during sleep)
 * - Speech production requires motor cortex activation (atonia during REM)
 * - Wernicke's area shows reduced semantic processing during sleep
 * - Sleep talking in REM demonstrates partial speech production capability
 */
static void speech_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    speech_sleep_bridge_t bridge = (speech_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Speech cortex bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_phoneme_modulation) {
        float phoneme_base = speech_sleep_get_phoneme_factor(new_state);
        bridge->effects.phoneme_discrimination_factor =
            1.0f + (phoneme_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_clarity_modulation) {
        float clarity_base = speech_sleep_get_clarity_factor(new_state);
        bridge->effects.speech_clarity_factor =
            1.0f + (clarity_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_comprehension_modulation) {
        float comprehension_base = speech_sleep_get_comprehension_factor(new_state);
        bridge->effects.comprehension_factor =
            1.0f + (comprehension_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.speech_processing_enabled = (new_state != SLEEP_STATE_DEEP_NREM) ||
                                                  bridge->effects.phoneme_discrimination_factor > 0.2f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Speech modulated: phoneme=%.2f, clarity=%.2f, comprehension=%.2f",
                        bridge->effects.phoneme_discrimination_factor,
                        bridge->effects.speech_clarity_factor,
                        bridge->effects.comprehension_factor);
}

int speech_sleep_default_config(speech_sleep_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "speech_sleep_default_config: NULL config");
    config->enable_phoneme_modulation = true;
    config->enable_clarity_modulation = true;
    config->enable_comprehension_modulation = true;
    config->modulation_strength = 1.0f;
    return 0;
}

speech_sleep_bridge_t speech_sleep_bridge_create(
    const speech_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    NIMCP_API_CHECK_NULL_RET_NULL(sleep_system, "speech_sleep_bridge_create: NULL sleep_system");

    struct speech_sleep_bridge_struct* bridge =
        (struct speech_sleep_bridge_struct*)nimcp_malloc(sizeof(struct speech_sleep_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "speech_sleep_bridge_create: Failed to allocate bridge");

    memset(bridge, 0, sizeof(struct speech_sleep_bridge_struct));

    if (config) {
        bridge->config = *config;
    } else {
        speech_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->effects.phoneme_discrimination_factor = 1.0f;
    bridge->effects.speech_clarity_factor = 1.0f;
    bridge->effects.comprehension_factor = 1.0f;
    bridge->effects.speech_processing_enabled = true;

    if (bridge_base_init(&bridge->base, 0, "speech_cortex_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        speech_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for speech cortex bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    speech_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Speech cortex-sleep bridge created");
    return bridge;
}

void speech_sleep_bridge_destroy(speech_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            speech_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for speech cortex bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int speech_sleep_update(speech_sleep_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "speech_sleep_update: NULL bridge");

    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_phoneme_modulation) {
        float phoneme_base = speech_sleep_get_phoneme_factor(state);
        bridge->effects.phoneme_discrimination_factor =
            1.0f + (phoneme_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_clarity_modulation) {
        float clarity_base = speech_sleep_get_clarity_factor(state);
        bridge->effects.speech_clarity_factor =
            1.0f + (clarity_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_comprehension_modulation) {
        float comprehension_base = speech_sleep_get_comprehension_factor(state);
        bridge->effects.comprehension_factor =
            1.0f + (comprehension_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.speech_processing_enabled = (state != SLEEP_STATE_DEEP_NREM) ||
                                                  bridge->effects.phoneme_discrimination_factor > 0.2f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int speech_sleep_get_effects(const speech_sleep_bridge_t bridge, speech_sleep_effects_t* effects) {
    NIMCP_API_CHECK_NULL(bridge, -1, "speech_sleep_get_effects: NULL bridge");
    NIMCP_API_CHECK_NULL(effects, -1, "speech_sleep_get_effects: NULL effects");
    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float speech_sleep_get_phoneme_discrimination(const speech_sleep_bridge_t bridge, float base_discrimination) {
    if (!bridge) return base_discrimination;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = base_discrimination * bridge->effects.phoneme_discrimination_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float speech_sleep_get_speech_clarity(const speech_sleep_bridge_t bridge, float base_clarity) {
    if (!bridge) return base_clarity;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = base_clarity * bridge->effects.speech_clarity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float speech_sleep_get_comprehension(const speech_sleep_bridge_t bridge, float base_comprehension) {
    if (!bridge) return base_comprehension;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = base_comprehension * bridge->effects.comprehension_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float speech_sleep_get_phoneme_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return SPEECH_SLEEP_PHONEME_AWAKE;
        case SLEEP_STATE_DROWSY:     return SPEECH_SLEEP_PHONEME_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return SPEECH_SLEEP_PHONEME_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return SPEECH_SLEEP_PHONEME_DEEP_NREM;
        case SLEEP_STATE_REM:        return SPEECH_SLEEP_PHONEME_REM;
        default:                     return SPEECH_SLEEP_PHONEME_AWAKE;
    }
}

float speech_sleep_get_clarity_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return SPEECH_SLEEP_CLARITY_AWAKE;
        case SLEEP_STATE_DROWSY:     return SPEECH_SLEEP_CLARITY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return SPEECH_SLEEP_CLARITY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return SPEECH_SLEEP_CLARITY_DEEP_NREM;
        case SLEEP_STATE_REM:        return SPEECH_SLEEP_CLARITY_REM;
        default:                     return SPEECH_SLEEP_CLARITY_AWAKE;
    }
}

float speech_sleep_get_comprehension_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return SPEECH_SLEEP_COMPREHEN_AWAKE;
        case SLEEP_STATE_DROWSY:     return SPEECH_SLEEP_COMPREHEN_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return SPEECH_SLEEP_COMPREHEN_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return SPEECH_SLEEP_COMPREHEN_DEEP_NREM;
        case SLEEP_STATE_REM:        return SPEECH_SLEEP_COMPREHEN_REM;
        default:                     return SPEECH_SLEEP_COMPREHEN_AWAKE;
    }
}
