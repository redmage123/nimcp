/**
 * @file nimcp_multimodal_nlp_immune_bridge.c
 * @brief Multimodal NLP-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "nlp/immune/nimcp_multimodal_nlp_immune_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(multimodal_nlp_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static float get_inflammation_multimodal_capacity(brain_inflammation_level_t level) {
    return INFLAMMATION_MULTIMODAL_BASE -
           (float)level * INFLAMMATION_MULTIMODAL_PER_LEVEL;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int multimodal_nlp_immune_default_config(multimodal_nlp_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_default_config: NULL config");
        return -1;
    }

    config->enable_cytokine_multimodal_impairment = true;
    config->enable_inflammation_binding_errors = true;
    config->enable_binding_failure_inflammation = true;
    config->enable_speech_error_inflammation = true;
    config->enable_fluent_speech_il10 = true;

    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->error_immune_sensitivity = 1.0f;

    config->binding_failure_threshold = MULTIMODAL_BINDING_THRESHOLD;
    config->speech_error_threshold = SPEECH_ERROR_IMMUNE_THRESHOLD;
    config->fluency_success_threshold = 0.8f;

    return 0;
}

multimodal_nlp_immune_bridge_t* multimodal_nlp_immune_bridge_create(
    const multimodal_nlp_immune_config_t* config,
    brain_immune_system_t* immune_system,
    speech_cortex_t* speech_cortex,
    visual_cortex_t* visual_cortex,
    audio_cortex_t* audio_cortex
) {
    if (!immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_bridge_create: immune_system required");
        NIMCP_LOGGING_ERROR("multimodal_nlp_immune_bridge_create: immune_system required");
        return NULL;
    }

    multimodal_nlp_immune_bridge_t* bridge = nimcp_malloc(sizeof(multimodal_nlp_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multimodal_nlp_immune_bridge_create: allocation failed");
        NIMCP_LOGGING_ERROR("multimodal_nlp_immune_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(multimodal_nlp_immune_bridge_t));

    multimodal_nlp_immune_config_t default_config;
    if (!config) {
        multimodal_nlp_immune_default_config(&default_config);
        config = &default_config;
    }

    bridge->config = *config;
    bridge->immune_system = immune_system;
    bridge->speech_cortex = speech_cortex;
    bridge->visual_cortex = visual_cortex;
    bridge->audio_cortex = audio_cortex;
    bridge->last_update_time = get_time_ms();

    nimcp_mutex_t* mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (mutex) {
        nimcp_mutex_init(mutex, NULL);
        bridge->base.mutex = mutex;
    }

    NIMCP_LOGGING_INFO("multimodal_nlp_immune_bridge: created successfully");
    return bridge;
}

void multimodal_nlp_immune_bridge_destroy(multimodal_nlp_immune_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        multimodal_nlp_immune_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        nimcp_mutex_t* mutex = (nimcp_mutex_t*)bridge->base.mutex;
        nimcp_mutex_destroy(mutex);
        nimcp_free(mutex);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → Multimodal NLP API
 * ============================================================================ */

int multimodal_nlp_immune_apply_cytokine_effects(multimodal_nlp_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_apply_cytokine_effects: NULL bridge or immune_system");
        return -1;
    }

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "multimodal_nlp_immune_apply_cytokine_effects: failed to get immune stats");
        return -1;
    }

    float inflammation_factor = (float)stats.inflammation_sites / 10.0f;
    inflammation_factor = nimcp_clamp01(inflammation_factor);

    bridge->cytokine_effects.il1_multimodal_deficit =
        CYTOKINE_IL1_MULTIMODAL_IMPACT * inflammation_factor * bridge->config.cytokine_sensitivity;
    bridge->cytokine_effects.tnf_multimodal_deficit =
        CYTOKINE_TNF_MULTIMODAL_IMPACT * inflammation_factor * bridge->config.cytokine_sensitivity;
    bridge->cytokine_effects.il10_multimodal_recovery =
        CYTOKINE_IL10_MULTIMODAL_IMPACT * (1.0f - inflammation_factor);

    bridge->cytokine_effects.total_integration_impairment = nimcp_clamp01(
        -(bridge->cytokine_effects.il1_multimodal_deficit +
          bridge->cytokine_effects.tnf_multimodal_deficit -
          bridge->cytokine_effects.il10_multimodal_recovery)
    );

    bridge->cytokine_effects.speech_production_deficit =
        bridge->cytokine_effects.total_integration_impairment * 0.9f;
    bridge->cytokine_effects.phonological_impairment =
        bridge->cytokine_effects.total_integration_impairment * 0.7f;

    bridge->cytokine_impairments++;
    return 0;
}

int multimodal_nlp_immune_apply_inflammation_effects(multimodal_nlp_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_apply_inflammation_effects: NULL bridge or immune_system");
        return -1;
    }

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "multimodal_nlp_immune_apply_inflammation_effects: failed to get immune stats");
        return -1;
    }

    brain_inflammation_level_t level;
    if (stats.inflammation_sites < 3) {
        level = INFLAMMATION_LOCAL;
    } else if (stats.inflammation_sites < 10) {
        level = INFLAMMATION_REGIONAL;
    } else {
        level = INFLAMMATION_SYSTEMIC;
    }

    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.integration_capacity =
        get_inflammation_multimodal_capacity(level);

    bridge->inflammation_state.speech_fluency_factor =
        bridge->inflammation_state.integration_capacity;
    bridge->inflammation_state.binding_error_rate =
        1.0f - bridge->inflammation_state.integration_capacity;

    return 0;
}

float multimodal_nlp_immune_compute_integration_capacity(
    const multimodal_nlp_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;

    float cytokine_factor = 1.0f - bridge->cytokine_effects.total_integration_impairment;
    float inflammation_factor = bridge->inflammation_state.integration_capacity;

    return cytokine_factor * inflammation_factor;
}

float multimodal_nlp_immune_compute_speech_fluency(
    const multimodal_nlp_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;

    float cytokine_factor = 1.0f - bridge->cytokine_effects.speech_production_deficit;
    float inflammation_factor = bridge->inflammation_state.speech_fluency_factor;

    return cytokine_factor * inflammation_factor;
}

/* ============================================================================
 * Multimodal NLP → Immune API
 * ============================================================================ */

int multimodal_nlp_immune_trigger_binding_failure_inflammation(
    multimodal_nlp_immune_bridge_t* bridge,
    float binding_error_rate
) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_trigger_binding_failure_inflammation: NULL bridge or immune_system");
        return -1;
    }
    if (!bridge->config.enable_binding_failure_inflammation) return 0;

    if (binding_error_rate < (1.0f - bridge->config.binding_failure_threshold)) {
        return 0;
    }

    uint32_t cytokine_id;
    float concentration = binding_error_rate * bridge->config.error_immune_sensitivity;
    concentration = nimcp_clamp01(concentration);

    int result = brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL6,
        0,
        concentration,
        0,
        &cytokine_id
    );

    if (result == 0) {
        bridge->binding_error_triggers++;
        bridge->multimodal_modulation.binding_failure_inflammation = true;
    }

    return result;
}

int multimodal_nlp_immune_trigger_speech_error_inflammation(
    multimodal_nlp_immune_bridge_t* bridge,
    float speech_error_rate
) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_trigger_speech_error_inflammation: NULL bridge or immune_system");
        return -1;
    }
    if (!bridge->config.enable_speech_error_inflammation) return 0;

    if (speech_error_rate < bridge->config.speech_error_threshold) {
        return 0;
    }

    uint32_t cytokine_id;
    float concentration = speech_error_rate * bridge->config.error_immune_sensitivity;
    concentration = nimcp_clamp01(concentration);

    int result = brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL1,
        0,
        concentration,
        0,
        &cytokine_id
    );

    if (result == 0) {
        bridge->speech_error_triggers++;
        bridge->multimodal_modulation.speech_error_inflammation = true;
    }

    return result;
}

int multimodal_nlp_immune_release_il10_from_fluent_speech(
    multimodal_nlp_immune_bridge_t* bridge,
    float fluency_rate
) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_release_il10_from_fluent_speech: NULL bridge or immune_system");
        return -1;
    }
    if (!bridge->config.enable_fluent_speech_il10) return 0;

    if (fluency_rate < bridge->config.fluency_success_threshold) {
        return 0;
    }

    uint32_t cytokine_id;
    float concentration = (fluency_rate - bridge->config.fluency_success_threshold) * 0.5f;
    concentration = nimcp_clamp01(concentration);

    int result = brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL10,
        0,
        concentration,
        0,
        &cytokine_id
    );

    if (result == 0) {
        bridge->fluency_boosts++;
        bridge->multimodal_modulation.il10_from_fluent_speech = concentration;
    }

    return result;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int multimodal_nlp_immune_bridge_update(
    multimodal_nlp_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_bridge_update: NULL bridge");
        return -1;
    }

    nimcp_mutex_t* mutex = (nimcp_mutex_t*)bridge->base.mutex;
    if (mutex) nimcp_mutex_lock(mutex);

    if (bridge->inflammation_state.current_level != INFLAMMATION_NONE) {
        bridge->inflammation_state.inflammation_duration_sec += (float)delta_ms / 1000.0f;
    } else {
        bridge->inflammation_state.inflammation_duration_sec = 0.0f;
    }

    multimodal_nlp_immune_apply_cytokine_effects(bridge);
    multimodal_nlp_immune_apply_inflammation_effects(bridge);

    if (bridge->multimodal_modulation.total_multimodal_inputs > 0) {
        bridge->multimodal_modulation.binding_success_rate =
            (float)bridge->multimodal_modulation.successful_integrations /
            (float)bridge->multimodal_modulation.total_multimodal_inputs;
    }

    bridge->total_updates++;
    bridge->last_update_time = get_time_ms();

    if (mutex) nimcp_mutex_unlock(mutex);
    return 0;
}

int multimodal_nlp_immune_apply_modulation(
    multimodal_nlp_immune_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_apply_modulation: NULL bridge");
        return -1;
    }
    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int multimodal_nlp_immune_get_cytokine_effects(
    const multimodal_nlp_immune_bridge_t* bridge,
    multimodal_cytokine_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_get_cytokine_effects: NULL bridge or effects");
        return -1;
    }
    *effects = bridge->cytokine_effects;
    return 0;
}

int multimodal_nlp_immune_get_inflammation_state(
    const multimodal_nlp_immune_bridge_t* bridge,
    multimodal_inflammation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_get_inflammation_state: NULL bridge or state");
        return -1;
    }
    *state = bridge->inflammation_state;
    return 0;
}

bool multimodal_nlp_immune_has_integration_deficit(
    const multimodal_nlp_immune_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    float capacity = multimodal_nlp_immune_compute_integration_capacity(bridge);
    return capacity < 0.7f;
}

float multimodal_nlp_immune_get_integration_capacity(
    const multimodal_nlp_immune_bridge_t* bridge
) {
    return multimodal_nlp_immune_compute_integration_capacity(bridge);
}

float multimodal_nlp_immune_get_binding_error_rate(
    const multimodal_nlp_immune_bridge_t* bridge
) {
    return bridge ? bridge->inflammation_state.binding_error_rate : 0.0f;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int multimodal_nlp_immune_connect_bio_async(
    multimodal_nlp_immune_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_connect_bio_async: NULL bridge");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_MULTIMODAL_NLP,
        .module_name = "multimodal_nlp_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("multimodal_nlp_immune_bridge: connected to bio-async router");
    } else {
        NIMCP_LOGGING_DEBUG("multimodal_nlp_immune_bridge: bio-async router not available");
    }
    return 0;
}

int multimodal_nlp_immune_disconnect_bio_async(
    multimodal_nlp_immune_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_immune_disconnect_bio_async: NULL bridge");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("multimodal_nlp_immune_bridge: bio-async disconnected");
    return 0;
}

bool multimodal_nlp_immune_is_bio_async_connected(
    const multimodal_nlp_immune_bridge_t* bridge
) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
