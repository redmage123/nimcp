/**
 * @file nimcp_amygdala_attention_bridge.c
 * @brief Amygdala-Attention Integration Bridge Implementation
 */

#include "core/brain/subcortical/nimcp_amygdala_attention_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(amygdala_attention_bridge, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* BIO_MODULE_AMYGDALA_ATTENTION defined in nimcp_bio_messages.h */

/* ============================================================================
 * Configuration Functions
 * ============================================================================ */

int amygdala_attention_default_config(amygdala_attention_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->enable_threat_salience_boost = true;
    config->enable_hypervigilance_mode = AMYG_ATT_DEFAULT_HYPERVIGILANCE_ENABLED;
    config->enable_attention_enhancement = true;
    config->enable_distraction_suppression = true;

    config->threat_sensitivity = AMYG_ATT_DEFAULT_THREAT_SENSITIVITY;
    config->attention_sensitivity = AMYG_ATT_DEFAULT_ATTENTION_SENSITIVITY;

    config->hypervigilance_threshold = AMYG_ATT_HYPERVIGILANCE_THRESHOLD;
    config->attention_focus_threshold = 0.5f;

    return 0;
}

int amygdala_attention_validate_config(const amygdala_attention_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    NIMCP_CHECK_THROW(config->threat_sensitivity >= 0.5f && config->threat_sensitivity <= 2.0f,
                      NIMCP_ERROR_INVALID_PARAM, "threat_sensitivity must be between 0.5 and 2.0");
    NIMCP_CHECK_THROW(config->attention_sensitivity >= 0.5f && config->attention_sensitivity <= 2.0f,
                      NIMCP_ERROR_INVALID_PARAM, "attention_sensitivity must be between 0.5 and 2.0");
    NIMCP_CHECK_THROW(config->hypervigilance_threshold >= 0.5f && config->hypervigilance_threshold <= 0.9f,
                      NIMCP_ERROR_INVALID_PARAM, "hypervigilance_threshold must be between 0.5 and 0.9");
    NIMCP_CHECK_THROW(config->attention_focus_threshold >= 0.3f && config->attention_focus_threshold <= 0.7f,
                      NIMCP_ERROR_INVALID_PARAM, "attention_focus_threshold must be between 0.3 and 0.7");

    return 0;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

amygdala_attention_bridge_t* amygdala_attention_create(
    const amygdala_attention_config_t* config
) {
    amygdala_attention_bridge_t* bridge = nimcp_malloc(sizeof(amygdala_attention_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate amygdala-attention bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(amygdala_attention_bridge_t));

    /* Initialize configuration */
    if (config) {
        if (amygdala_attention_validate_config(config) != 0) {
            NIMCP_LOGGING_ERROR("Invalid configuration");
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amygdala_attention_create: validation failed");
            return NULL;
        }
        bridge->config = *config;
    } else {
        amygdala_attention_default_config(&bridge->config);
    }

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "amygdala_attention") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created amygdala-attention bridge");
    return bridge;
}

void amygdala_attention_destroy(amygdala_attention_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed amygdala-attention bridge");
}

int amygdala_attention_reset(amygdala_attention_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset state */
    memset(&bridge->amygdala_effects, 0, sizeof(amygdala_attention_effects_t));
    memset(&bridge->attention_effects, 0, sizeof(attention_amygdala_effects_t));

    /* Reset statistics */
    bridge->total_updates = 0;
    bridge->base.total_updates = 0;
    bridge->hypervigilance_activations = 0;
    bridge->threat_boosts = 0;
    bridge->attention_enhancements = 0;
    bridge->last_update_time = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Reset amygdala-attention bridge");
    return 0;
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int amygdala_attention_connect_amygdala(
    amygdala_attention_bridge_t* bridge,
    amygdala_t* amygdala
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(amygdala, NIMCP_ERROR_NULL_POINTER, "amygdala is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->amygdala = amygdala;
    bridge->base.system_a = amygdala;
    bridge->amygdala_connected = true;
    bridge->base.system_a_connected = true;
    bridge->bridge_active = bridge->amygdala_connected && bridge->attention_connected;
    bridge->base.bridge_active = bridge->bridge_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected amygdala to bridge");
    return 0;
}

int amygdala_attention_connect_attention(
    amygdala_attention_bridge_t* bridge,
    void* attention
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(attention, NIMCP_ERROR_NULL_POINTER, "attention is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention = attention;
    bridge->base.system_b = attention;
    bridge->attention_connected = true;
    bridge->base.system_b_connected = true;
    bridge->bridge_active = bridge->amygdala_connected && bridge->attention_connected;
    bridge->base.bridge_active = bridge->bridge_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected attention to bridge");
    return 0;
}

int amygdala_attention_disconnect_amygdala(amygdala_attention_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->amygdala = NULL;
    bridge->amygdala_connected = false;
    bridge->base.system_a_connected = false;
    bridge->bridge_active = false;
    bridge->base.bridge_active = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected amygdala from bridge");
    return 0;
}

int amygdala_attention_disconnect_attention(amygdala_attention_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention = NULL;
    bridge->attention_connected = false;
    bridge->bridge_active = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected attention from bridge");
    return 0;
}

bool amygdala_attention_is_connected(const amygdala_attention_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bridge_active;
}

/* ============================================================================
 * Amygdala → Attention Functions
 * ============================================================================ */

int amygdala_attention_apply_amygdala_effects(amygdala_attention_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->amygdala_connected, NIMCP_ERROR_INVALID_STATE, "amygdala is not connected");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get amygdala state */
    bridge->amygdala_effects.fear_level = amygdala_get_fear_level(bridge->amygdala);
    bridge->amygdala_effects.anxiety_level = amygdala_get_anxiety_level(bridge->amygdala);
    bridge->amygdala_effects.threat_level = amygdala_get_threat_level(bridge->amygdala);

    /* Compute threat salience boost */
    if (bridge->config.enable_threat_salience_boost) {
        bridge->amygdala_effects.threat_salience_boost =
            amygdala_attention_compute_threat_boost(bridge);
        if (bridge->amygdala_effects.threat_salience_boost > 0.0f) {
            bridge->threat_boosts++;
        }
    }

    /* Compute vigilance level */
    float fear_contrib = bridge->amygdala_effects.fear_level * AMYG_ATT_FEAR_SALIENCE_SCALE;
    float anxiety_contrib = bridge->amygdala_effects.anxiety_level * AMYG_ATT_ANXIETY_VIGILANCE_SCALE;
    bridge->amygdala_effects.vigilance_level = fminf(1.0f, fear_contrib + anxiety_contrib);

    /* Check hypervigilance */
    if (bridge->config.enable_hypervigilance_mode) {
        bool was_hypervigilant = bridge->amygdala_effects.hypervigilance_active;
        bridge->amygdala_effects.hypervigilance_active =
            amygdala_attention_is_hypervigilant(bridge);

        if (!was_hypervigilant && bridge->amygdala_effects.hypervigilance_active) {
            bridge->hypervigilance_activations++;
        }
    }

    /* Compute disengagement difficulty */
    bridge->amygdala_effects.disengagement_difficulty =
        amygdala_attention_compute_disengagement_difficulty(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float amygdala_attention_compute_threat_boost(
    const amygdala_attention_bridge_t* bridge
) {
    if (!bridge) {
        return 0.0f;
    }

    float boost = 0.0f;

    /* Map threat level to boost factor */
    switch (bridge->amygdala_effects.threat_level) {
        case AMYG_THREAT_NONE:
            boost = AMYG_ATT_THREAT_NONE_BOOST;
            break;
        case AMYG_THREAT_LOW:
            boost = AMYG_ATT_THREAT_LOW_BOOST;
            break;
        case AMYG_THREAT_MODERATE:
            boost = AMYG_ATT_THREAT_MODERATE_BOOST;
            break;
        case AMYG_THREAT_HIGH:
            boost = AMYG_ATT_THREAT_HIGH_BOOST;
            break;
        case AMYG_THREAT_SEVERE:
            boost = AMYG_ATT_THREAT_SEVERE_BOOST;
            break;
        default:
            boost = 0.0f;
            break;
    }

    /* Apply fear modulation */
    boost += bridge->amygdala_effects.fear_level * AMYG_ATT_FEAR_SALIENCE_SCALE;

    /* Apply sensitivity */
    boost *= bridge->config.threat_sensitivity;

    return fminf(1.0f, boost);
}

bool amygdala_attention_is_hypervigilant(
    const amygdala_attention_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }

    return bridge->amygdala_effects.anxiety_level >= bridge->config.hypervigilance_threshold;
}

float amygdala_attention_compute_disengagement_difficulty(
    const amygdala_attention_bridge_t* bridge
) {
    if (!bridge) {
        return 0.0f;
    }

    float difficulty = AMYG_ATT_DISENGAGEMENT_IMPAIRMENT_BASE;
    difficulty += bridge->amygdala_effects.fear_level * AMYG_ATT_DISENGAGEMENT_PER_FEAR;

    /* Hypervigilance further impairs disengagement */
    if (bridge->amygdala_effects.hypervigilance_active) {
        difficulty *= 1.5f;
    }

    return fminf(1.0f, difficulty);
}

/* ============================================================================
 * Attention → Amygdala Functions
 * ============================================================================ */

int amygdala_attention_apply_attention_effects(amygdala_attention_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->attention_connected, NIMCP_ERROR_INVALID_STATE, "attention is not connected");

    nimcp_mutex_lock(bridge->base.mutex);

    /* NOTE: Attention system is opaque pointer, so we can't query its state directly.
     * In a real implementation, this would call attention system API to get:
     * - attention_strength
     * - focus_on_threat
     * - focus_on_neutral
     * For now, we use placeholder values that would be set externally.
     */

    /* Compute attention enhancement */
    if (bridge->config.enable_attention_enhancement) {
        bridge->attention_effects.attention_enhancement =
            amygdala_attention_compute_attention_enhancement(bridge);
        if (bridge->attention_effects.attention_enhancement > 0.0f) {
            bridge->attention_enhancements++;
        }
    }

    /* Compute distraction suppression */
    if (bridge->config.enable_distraction_suppression) {
        bridge->attention_effects.distraction_suppression =
            amygdala_attention_compute_distraction_suppression(bridge);
    }

    /* Compute prefrontal regulation */
    float focus_ratio = (bridge->attention_effects.focus_on_neutral > 0.001f) ?
        bridge->attention_effects.focus_on_neutral /
        (bridge->attention_effects.focus_on_threat + bridge->attention_effects.focus_on_neutral + 0.001f) : 0.0f;
    bridge->attention_effects.prefrontal_regulation = focus_ratio;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float amygdala_attention_compute_attention_enhancement(
    const amygdala_attention_bridge_t* bridge
) {
    if (!bridge) {
        return 0.0f;
    }

    /* Attention to threat enhances amygdala processing */
    float enhancement = AMYG_ATT_ATTENTION_ENHANCEMENT_BASE;
    enhancement += bridge->attention_effects.attention_strength * AMYG_ATT_FOCUS_ENHANCEMENT_SCALE;
    enhancement *= bridge->config.attention_sensitivity;

    return fminf(1.0f, enhancement);
}

float amygdala_attention_compute_distraction_suppression(
    const amygdala_attention_bridge_t* bridge
) {
    if (!bridge) {
        return 0.0f;
    }

    /* Attention to neutral stimuli suppresses amygdala */
    float suppression = bridge->attention_effects.focus_on_neutral * AMYG_ATT_DISTRACTION_SUPPRESSION;
    suppression *= bridge->config.attention_sensitivity;

    return fminf(1.0f, suppression);
}

/* ============================================================================
 * Update Functions
 * ============================================================================ */

int amygdala_attention_update(amygdala_attention_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->bridge_active, NIMCP_ERROR_INVALID_STATE, "bridge is not active");

    /* Apply amygdala → attention effects */
    int result = amygdala_attention_apply_amygdala_effects(bridge);
    if (result != 0) {
        return result;
    }

    /* Apply attention → amygdala effects */
    result = amygdala_attention_apply_attention_effects(bridge);
    if (result != 0) {
        return result;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->total_updates++;
    bridge->base.total_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int amygdala_attention_apply_modulation(amygdala_attention_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->bridge_active, NIMCP_ERROR_INVALID_STATE, "bridge is not active");

    /* NOTE: Actual modulation would be applied here.
     * For amygdala: adjust prefrontal inhibition based on attention
     * For attention: adjust threat salience based on amygdala state
     *
     * Since attention system is opaque, the bridge exposes computed effects
     * via getter functions that the attention system can query.
     *
     * For amygdala, we can directly apply prefrontal regulation:
     */
    if (bridge->amygdala_connected && bridge->attention_effects.prefrontal_regulation > 0.0f) {
        amygdala_set_prefrontal_inhibition(
            bridge->amygdala,
            bridge->attention_effects.prefrontal_regulation
        );
    }

    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

int amygdala_attention_get_amygdala_effects(
    const amygdala_attention_bridge_t* bridge,
    amygdala_attention_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    *effects = bridge->amygdala_effects;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int amygdala_attention_get_attention_effects(
    const amygdala_attention_bridge_t* bridge,
    attention_amygdala_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    *effects = bridge->attention_effects;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

float amygdala_attention_get_threat_salience_boost(
    const amygdala_attention_bridge_t* bridge
) {
    if (!bridge) {
        return 0.0f;
    }
    return bridge->amygdala_effects.threat_salience_boost;
}

float amygdala_attention_get_attention_enhancement(
    const amygdala_attention_bridge_t* bridge
) {
    if (!bridge) {
        return 0.0f;
    }
    return bridge->attention_effects.attention_enhancement;
}

int amygdala_attention_get_statistics(
    const amygdala_attention_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* hypervigilance_activations,
    uint32_t* threat_boosts,
    uint32_t* attention_enhancements
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    if (total_updates) {
        *total_updates = bridge->total_updates;
    }
    if (hypervigilance_activations) {
        *hypervigilance_activations = bridge->hypervigilance_activations;
    }
    if (threat_boosts) {
        *threat_boosts = bridge->threat_boosts;
    }
    if (attention_enhancements) {
        *attention_enhancements = bridge->attention_enhancements;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Functions
 * ============================================================================ */

int amygdala_attention_connect_bio_async(amygdala_attention_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return 0; /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_AMYGDALA_ATTENTION,
        .module_name = "amygdala_attention_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return 0;
}

int amygdala_attention_disconnect_bio_async(amygdala_attention_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return 0; /* Not connected */
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool amygdala_attention_is_bio_async_connected(
    const amygdala_attention_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
