/**
 * @file nimcp_nlp_immune_bridge.c
 * @brief NLP-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "nlp/immune/nimcp_nlp_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(nlp_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Get system time for duration tracking
 * WHY:  Track chronic inflammation and sustained errors
 * HOW:  Platform-specific time retrieval
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Get inflammation language capacity factor
 *
 * WHAT: Map inflammation level to language capacity reduction
 * WHY:  Different inflammation levels have different language impacts
 * HOW:  Return predefined factor based on level
 */
static float get_inflammation_language_capacity(brain_inflammation_level_t level) {
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_NONE_LANG_CAPACITY,
        INFLAMMATION_STORM_LANG_CAPACITY);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int nlp_immune_default_config(nlp_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "nlp_immune_default_config: NULL config");
        return -1;
    }

    /* Enable all features by default */
    config->enable_cytokine_language_impairment = true;
    config->enable_inflammation_errors = true;
    config->enable_error_immune_activation = true;
    config->enable_success_il10_release = true;
    config->enable_complexity_inflammation = true;

    /* Default sensitivity (1.0 = normal) */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->error_immune_sensitivity = 1.0f;

    /* Default thresholds */
    config->error_threshold = LANGUAGE_ERROR_IMMUNE_THRESHOLD;
    config->complexity_threshold = LANGUAGE_COMPLEXITY_THRESHOLD;
    config->success_threshold = 0.8f;

    return 0;
}

nlp_immune_bridge_t* nlp_immune_bridge_create(
    const nlp_immune_config_t* config,
    brain_immune_system_t* immune_system,
    nlp_network_t nlp_network
) {
    /* Guard: require immune system and NLP network */
    if (!immune_system || !nlp_network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_immune_bridge_create: immune_system and nlp_network required");
        NIMCP_LOGGING_ERROR("nlp_immune_bridge_create: immune_system and nlp_network required");
        return NULL;
    }

    /* Allocate bridge */
    nlp_immune_bridge_t* bridge = nimcp_malloc(sizeof(nlp_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nlp_immune_bridge_create: allocation failed");
        NIMCP_LOGGING_ERROR("nlp_immune_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(nlp_immune_bridge_t));

    /* Apply configuration */
    nlp_immune_config_t default_config;
    if (!config) {
        nlp_immune_default_config(&default_config);
        config = &default_config;
    }

    bridge->config = *config;
    bridge->enable_cytokine_language_impairment = config->enable_cytokine_language_impairment;
    bridge->enable_inflammation_errors = config->enable_inflammation_errors;
    bridge->enable_error_immune_activation = config->enable_error_immune_activation;
    bridge->enable_success_il10_release = config->enable_success_il10_release;
    bridge->enable_complexity_inflammation = config->enable_complexity_inflammation;

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->nlp_network = nlp_network;

    /* Initialize timing */
    bridge->last_update_time = get_time_ms();

    /* Create mutex */
    nimcp_mutex_t* mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (mutex) {
        nimcp_mutex_init(mutex, NULL);
        bridge->base.mutex = mutex;
    }

    NIMCP_LOGGING_INFO("nlp_immune_bridge: created successfully");
    return bridge;
}

void nlp_immune_bridge_destroy(nlp_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        nlp_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_mutex_t* mutex = (nimcp_mutex_t*)bridge->base.mutex;
        nimcp_mutex_destroy(mutex);
        nimcp_free(mutex);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → NLP API
 * ============================================================================ */

int nlp_immune_apply_cytokine_effects(nlp_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "nlp_immune_apply_cytokine_effects: NULL bridge or immune_system");
        return -1;
    }

    if (!bridge->enable_cytokine_language_impairment) {
        return 0;
    }

    /* Get immune stats to estimate cytokine levels */
    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "nlp_immune_apply_cytokine_effects: failed to get immune stats");
        return -1;
    }

    /* Compute cytokine effects (proportional to inflammation sites) */
    float inflammation_factor = (float)stats.inflammation_sites / 10.0f;
    inflammation_factor = nimcp_clamp01(inflammation_factor);

    /* Apply cytokine impacts */
    bridge->cytokine_effects.il1_language_deficit =
        CYTOKINE_IL1_LANGUAGE_IMPACT * inflammation_factor * bridge->config.cytokine_sensitivity;
    bridge->cytokine_effects.il6_language_deficit =
        CYTOKINE_IL6_LANGUAGE_IMPACT * inflammation_factor * bridge->config.cytokine_sensitivity;
    bridge->cytokine_effects.tnf_language_deficit =
        CYTOKINE_TNF_LANGUAGE_IMPACT * inflammation_factor * bridge->config.cytokine_sensitivity;
    bridge->cytokine_effects.ifn_gamma_language_deficit =
        CYTOKINE_IFN_GAMMA_LANGUAGE_IMPACT * inflammation_factor * bridge->config.cytokine_sensitivity;

    /* IL-10 provides recovery */
    bridge->cytokine_effects.il10_language_recovery =
        CYTOKINE_IL10_LANGUAGE_IMPACT * (1.0f - inflammation_factor);

    /* Aggregate effects */
    bridge->cytokine_effects.total_capacity_reduction = nimcp_clamp01(
        -(bridge->cytokine_effects.il1_language_deficit +
          bridge->cytokine_effects.il6_language_deficit +
          bridge->cytokine_effects.tnf_language_deficit +
          bridge->cytokine_effects.ifn_gamma_language_deficit -
          bridge->cytokine_effects.il10_language_recovery)
    );

    /* Specific impairments */
    bridge->cytokine_effects.vocab_access_impairment =
        bridge->cytokine_effects.total_capacity_reduction * 0.8f;
    bridge->cytokine_effects.semantic_error_rate =
        bridge->cytokine_effects.total_capacity_reduction * 0.6f;
    bridge->cytokine_effects.syntactic_impairment =
        bridge->cytokine_effects.total_capacity_reduction * 0.5f;
    bridge->cytokine_effects.comprehension_deficit =
        bridge->cytokine_effects.total_capacity_reduction * 0.7f;

    bridge->cytokine_impairments++;
    return 0;
}

int nlp_immune_apply_inflammation_effects(nlp_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "nlp_immune_apply_inflammation_effects: NULL bridge or immune_system");
        return -1;
    }

    if (!bridge->enable_inflammation_errors) {
        return 0;
    }

    /* Get current inflammation level */
    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "nlp_immune_apply_inflammation_effects: failed to get immune stats");
        return -1;
    }

    /* Estimate inflammation level from stats */
    brain_inflammation_level_t level;
    if (stats.inflammation_sites == 0) {
        level = INFLAMMATION_NONE;
    } else if (stats.inflammation_sites < 3) {
        level = INFLAMMATION_LOCAL;
    } else if (stats.inflammation_sites < 10) {
        level = INFLAMMATION_REGIONAL;
    } else if (stats.inflammation_sites < 20) {
        level = INFLAMMATION_SYSTEMIC;
    } else {
        level = INFLAMMATION_STORM;
    }

    bridge->inflammation_state.current_level = level;

    /* Compute capacity factor */
    bridge->inflammation_state.capacity_factor =
        get_inflammation_language_capacity(level) * bridge->config.inflammation_sensitivity;

    /* Compute error rate based on level */
    bridge->inflammation_state.error_rate =
        INFLAMMATION_ERROR_BASE +
        (float)level * INFLAMMATION_ERROR_PER_LEVEL * bridge->config.inflammation_sensitivity;
    bridge->inflammation_state.error_rate = nimcp_clamp01(bridge->inflammation_state.error_rate);

    /* Specific impairments */
    bridge->inflammation_state.vocab_reduction = 1.0f - bridge->inflammation_state.capacity_factor;
    bridge->inflammation_state.fluency_impairment = 1.0f - bridge->inflammation_state.capacity_factor;
    bridge->inflammation_state.complexity_reduction = 1.0f - bridge->inflammation_state.capacity_factor;

    return 0;
}

float nlp_immune_compute_capacity(const nlp_immune_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;
    }

    /* Combine cytokine and inflammation effects */
    float cytokine_factor = 1.0f - bridge->cytokine_effects.total_capacity_reduction;
    float inflammation_factor = bridge->inflammation_state.capacity_factor;

    return cytokine_factor * inflammation_factor;
}

float nlp_immune_compute_error_rate(const nlp_immune_bridge_t* bridge) {
    if (!bridge) {
        return 0.0f;
    }

    /* Combine semantic errors and inflammation errors */
    float total_error = bridge->cytokine_effects.semantic_error_rate +
                        bridge->inflammation_state.error_rate;
    return nimcp_clamp01(total_error);
}

/* ============================================================================
 * NLP → Immune API
 * ============================================================================ */

int nlp_immune_trigger_error_inflammation(
    nlp_immune_bridge_t* bridge,
    float error_rate
) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "nlp_immune_trigger_error_inflammation: NULL bridge or immune_system");
        return -1;
    }

    if (!bridge->enable_error_immune_activation) {
        return 0;
    }

    /* Check if error rate exceeds threshold */
    if (error_rate < bridge->config.error_threshold) {
        return 0;
    }

    /* Trigger IL-1β release (pro-inflammatory) */
    uint32_t cytokine_id;
    float concentration = (error_rate - bridge->config.error_threshold) *
                         bridge->config.error_immune_sensitivity;
    concentration = nimcp_clamp01(concentration);

    int result = brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL1,
        0,  /* source_cell (0 = general) */
        concentration,
        0,  /* target_region (0 = broadcast) */
        &cytokine_id
    );

    if (result == 0) {
        bridge->error_triggers++;
        bridge->nlp_modulation.error_induced_inflammation = true;
    }

    return result;
}

int nlp_immune_release_il10_from_success(
    nlp_immune_bridge_t* bridge,
    float success_rate
) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "nlp_immune_release_il10_from_success: NULL bridge or immune_system");
        return -1;
    }

    if (!bridge->enable_success_il10_release) {
        return 0;
    }

    /* Check if success rate exceeds threshold */
    if (success_rate < bridge->config.success_threshold) {
        return 0;
    }

    /* Trigger IL-10 release (anti-inflammatory) */
    uint32_t cytokine_id;
    float concentration = (success_rate - bridge->config.success_threshold) *
                         LANGUAGE_SUCCESS_IL10_BOOST;
    concentration = nimcp_clamp01(concentration);

    int result = brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL10,
        0,  /* source_cell */
        concentration,
        0,  /* target_region */
        &cytokine_id
    );

    if (result == 0) {
        bridge->success_boosts++;
        bridge->nlp_modulation.il10_release_from_success = concentration;
    }

    return result;
}

int nlp_immune_trigger_complexity_inflammation(
    nlp_immune_bridge_t* bridge,
    float complexity_level
) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "nlp_immune_trigger_complexity_inflammation: NULL bridge or immune_system");
        return -1;
    }

    if (!bridge->enable_complexity_inflammation) {
        return 0;
    }

    /* Check if complexity exceeds threshold */
    if (complexity_level < bridge->config.complexity_threshold) {
        return 0;
    }

    /* Trigger IL-6 release (cognitive load inflammation) */
    uint32_t cytokine_id;
    float concentration = (complexity_level - bridge->config.complexity_threshold) * 0.5f;
    concentration = nimcp_clamp01(concentration);

    int result = brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL6,
        0,  /* source_cell */
        concentration,
        0,  /* target_region */
        &cytokine_id
    );

    if (result == 0) {
        bridge->complexity_inflammation_events++;
        bridge->nlp_modulation.inflammation_from_complexity = concentration;
    }

    return result;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int nlp_immune_bridge_update(
    nlp_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "nlp_immune_bridge_update: NULL bridge");
        return -1;
    }

    nimcp_mutex_t* mutex = (nimcp_mutex_t*)bridge->base.mutex;
    if (mutex) {
        nimcp_mutex_lock(mutex);
    }

    /* Update inflammation duration */
    if (bridge->inflammation_state.current_level != INFLAMMATION_NONE) {
        bridge->inflammation_state.inflammation_duration_sec += (float)delta_ms / 1000.0f;
        bridge->inflammation_state.is_chronic =
            bridge->inflammation_state.inflammation_duration_sec > 300.0f;  /* 5 minutes */
    } else {
        bridge->inflammation_state.inflammation_duration_sec = 0.0f;
        bridge->inflammation_state.is_chronic = false;
    }

    /* Apply immune → NLP effects */
    nlp_immune_apply_cytokine_effects(bridge);
    nlp_immune_apply_inflammation_effects(bridge);

    /* Update NLP modulation state */
    if (bridge->nlp_modulation.total_processed > 0) {
        bridge->nlp_modulation.processing_success_rate =
            (float)bridge->nlp_modulation.successful_parses /
            (float)bridge->nlp_modulation.total_processed;
        bridge->nlp_modulation.error_rate =
            (float)bridge->nlp_modulation.failed_parses /
            (float)bridge->nlp_modulation.total_processed;
    }

    bridge->total_updates++;
    bridge->last_update_time = get_time_ms();

    if (mutex) {
        nimcp_mutex_unlock(mutex);
    }

    return 0;
}

int nlp_immune_apply_modulation(nlp_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge || !bridge->nlp_network) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "nlp_immune_apply_modulation: NULL bridge or nlp_network");
        return -1;
    }

    /* This would apply modulation to NLP network */
    /* Implementation depends on NLP network API */
    /* For now, just track that modulation was applied */

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int nlp_immune_get_cytokine_effects(
    const nlp_immune_bridge_t* bridge,
    nlp_cytokine_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "nlp_immune_get_cytokine_effects: NULL bridge or effects");
        return -1;
    }

    *effects = bridge->cytokine_effects;
    return 0;
}

int nlp_immune_get_inflammation_state(
    const nlp_immune_bridge_t* bridge,
    nlp_inflammation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "nlp_immune_get_inflammation_state: NULL bridge or state");
        return -1;
    }

    *state = bridge->inflammation_state;
    return 0;
}

bool nlp_immune_has_language_deficit(const nlp_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    float capacity = nlp_immune_compute_capacity(bridge);
    return capacity < 0.7f;  /* >30% capacity loss */
}

float nlp_immune_get_capacity_factor(const nlp_immune_bridge_t* bridge) {
    return nlp_immune_compute_capacity(bridge);
}

float nlp_immune_get_error_rate(const nlp_immune_bridge_t* bridge) {
    return nlp_immune_compute_error_rate(bridge);
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int nlp_immune_connect_bio_async(nlp_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "nlp_immune_connect_bio_async: NULL bridge");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0; /* Already connected */
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_NLP_CORE,
        .module_name = "nlp_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("nlp_immune_bridge: connected to bio-async router");
    } else {
        NIMCP_LOGGING_DEBUG("nlp_immune_bridge: bio-async router not available");
    }

    return 0;
}

int nlp_immune_disconnect_bio_async(nlp_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "nlp_immune_disconnect_bio_async: NULL bridge");
        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0; /* Already disconnected */
    }

    /* Unregister from bio-async router */
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("nlp_immune_bridge: bio-async disconnected");

    return 0;
}

bool nlp_immune_is_bio_async_connected(const nlp_immune_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
