/**
 * @file nimcp_cochlea_immune_bridge.c
 * @brief Cochlea-Immune system integration implementation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_immune_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_immune_bridge)

#define LOG_MODULE "COCHLEA_IMMUNE_BRIDGE"

//=============================================================================
// Internal Structure
//=============================================================================

#define COCHLEA_IMMUNE_MAX_CHANNELS 64

struct cochlea_immune_bridge {
    bridge_base_t base;                         /* MUST be first */
    cochlea_immune_config_t config;

    /* Connected systems */
    cochlea_t* cochlea;
    brain_immune_system_t* immune;

    /* Health state */
    cochlea_health_state_t health;

    /* Immune response */
    immune_response_t response;

    /* Recovery tracking */
    float recovery_progress;                    /* [0-1] */

    /* Bidirectional timestamps */
    uint64_t last_outbound_ts;
    uint64_t last_inbound_ts;
};

//=============================================================================
// Helpers
//=============================================================================

static uint64_t immune_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static inline float immune_clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

//=============================================================================
// Configuration
//=============================================================================

cochlea_immune_config_t cochlea_immune_config_default(void) {
    cochlea_immune_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.tts_threshold_db = 85.0f;
    cfg.pts_threshold_db = 100.0f;
    cfg.trauma_threshold_db = 120.0f;
    cfg.safe_exposure_time_min = 480.0f;  /* 8 hours at 85 dB */
    cfg.time_intensity_tradeoff = 3.0f;   /* 3 dB exchange rate */
    cfg.recovery_rate_per_hour = 0.1f;
    cfg.enable_permanent_damage = true;
    cfg.enable_immune_response = true;
    cfg.inflammation_threshold = 0.3f;
    return cfg;
}

//=============================================================================
// Core API
//=============================================================================

cochlea_immune_bridge_t* cochlea_immune_bridge_create(
    cochlea_t* cochlea,
    brain_immune_system_t* immune,
    const cochlea_immune_config_t* config
) {
    cochlea_immune_bridge_heartbeat("create", 0.0f);

    if (!cochlea) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_bridge_create: cochlea NULL");
        return NULL;
    }
    if (!immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_bridge_create: immune NULL");
        return NULL;
    }

    cochlea_immune_bridge_t* bridge = (cochlea_immune_bridge_t*)
        nimcp_calloc(1, sizeof(cochlea_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_immune_bridge_create: bridge is NULL");
        return NULL;
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_immune") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_immune_bridge_create: validation failed");
        return NULL;
    }

    /* Store references */
    bridge->cochlea = cochlea;
    bridge->immune = immune;

    /* Apply config */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cochlea_immune_config_default();
    }

    /* Initialize health to perfect */
    bridge->health.num_channels = COCHLEA_IMMUNE_MAX_CHANNELS;
    bridge->health.overall_health = 1.0f;
    bridge->health.damage_type = COCHLEA_DAMAGE_NONE;
    bridge->health.inflammation = INFLAMMATION_NONE;
    for (uint32_t i = 0; i < COCHLEA_IMMUNE_MAX_CHANNELS; i++) {
        bridge->health.ohc_survival[i] = 1.0f;
        bridge->health.ihc_survival[i] = 1.0f;
    }

    bridge->recovery_progress = 1.0f;

    bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    bridge_base_connect_b_unlocked(&bridge->base, immune);

    cochlea_immune_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_immune_bridge_destroy(cochlea_immune_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_immune");
    cochlea_immune_bridge_heartbeat("destroy", 0.0f);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_immune_bridge_update(
    cochlea_immune_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_bridge_update: bridge NULL");
        return -1;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_bridge_update: cochlea_output NULL");
        return -1;
    }

    cochlea_immune_bridge_heartbeat("update", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute aggregate health from per-channel survival */
    float health_sum = 0.0f;
    for (uint32_t i = 0; i < bridge->health.num_channels; i++) {
        health_sum += (bridge->health.ohc_survival[i] + bridge->health.ihc_survival[i]) * 0.5f;
    }
    if (bridge->health.num_channels > 0) {
        bridge->health.overall_health = health_sum / (float)bridge->health.num_channels;
    }

    /* Update inflammation based on oxidative stress */
    if (bridge->health.oxidative_stress > bridge->config.inflammation_threshold) {
        if (bridge->health.inflammation == INFLAMMATION_NONE) {
            bridge->health.inflammation = INFLAMMATION_ACUTE;
        }
        bridge->health.cytokine_level = immune_clampf(
            bridge->health.oxidative_stress * 0.8f, 0.0f, 1.0f);
    }

    /* If immune response active, apply recovery */
    if (bridge->response.response_active) {
        float recovery_delta = bridge->response.recovery_rate * (dt_ms / 3600000.0f);
        bridge->recovery_progress = immune_clampf(
            bridge->recovery_progress + recovery_delta, 0.0f, 1.0f);
    }

    bridge->last_outbound_ts = immune_get_time_ms();
    bridge->last_inbound_ts = bridge->last_outbound_ts;

    bridge_base_record_update(&bridge->base);
    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_immune_bridge_heartbeat("update", 1.0f);
    return 0;
}

nimcp_error_t cochlea_immune_bridge_reset(cochlea_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_bridge_reset: bridge NULL");
        return -1;
    }
    cochlea_immune_bridge_heartbeat("reset", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    /* Restore perfect health */
    for (uint32_t i = 0; i < COCHLEA_IMMUNE_MAX_CHANNELS; i++) {
        bridge->health.ohc_survival[i] = 1.0f;
        bridge->health.ihc_survival[i] = 1.0f;
    }
    bridge->health.overall_health = 1.0f;
    bridge->health.damage_type = COCHLEA_DAMAGE_NONE;
    bridge->health.inflammation = INFLAMMATION_NONE;
    bridge->health.oxidative_stress = 0.0f;
    bridge->health.cytokine_level = 0.0f;

    memset(&bridge->response, 0, sizeof(bridge->response));
    bridge->recovery_progress = 1.0f;
    bridge->last_outbound_ts = 0;
    bridge->last_inbound_ts = 0;

    bridge_base_reset_unlocked(&bridge->base);
    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_immune_bridge_heartbeat("reset", 1.0f);
    return 0;
}

//=============================================================================
// Damage and Health
//=============================================================================

nimcp_error_t cochlea_immune_get_health(
    const cochlea_immune_bridge_t* bridge,
    cochlea_health_state_t* health
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_get_health: bridge NULL");
        return -1;
    }
    if (!health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_get_health: health NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *health = bridge->health;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

nimcp_error_t cochlea_immune_apply_exposure(
    cochlea_immune_bridge_t* bridge,
    float level_db,
    float duration_min
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_apply_exposure: bridge NULL");
        return -1;
    }

    cochlea_immune_bridge_heartbeat("apply_exposure", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    /* Calculate damage based on level and duration */
    float damage_factor = 0.0f;

    if (level_db >= bridge->config.trauma_threshold_db) {
        /* Acoustic trauma */
        bridge->health.damage_type = COCHLEA_DAMAGE_ACOUSTIC_TRAUMA;
        damage_factor = 0.8f;
    } else if (level_db >= bridge->config.pts_threshold_db) {
        /* Permanent threshold shift */
        if (bridge->config.enable_permanent_damage) {
            bridge->health.damage_type = COCHLEA_DAMAGE_PERMANENT;
        }
        float excess_db = level_db - bridge->config.pts_threshold_db;
        float time_factor = duration_min / bridge->config.safe_exposure_time_min;
        damage_factor = immune_clampf(excess_db * time_factor * 0.01f, 0.0f, 0.6f);
    } else if (level_db >= bridge->config.tts_threshold_db) {
        /* Temporary threshold shift */
        bridge->health.damage_type = COCHLEA_DAMAGE_TEMPORARY;
        float excess_db = level_db - bridge->config.tts_threshold_db;
        /* Apply 3 dB exchange rate */
        float safe_time = bridge->config.safe_exposure_time_min *
            powf(2.0f, -(excess_db / bridge->config.time_intensity_tradeoff));
        float overexposure = duration_min / safe_time;
        damage_factor = immune_clampf(overexposure * 0.1f, 0.0f, 0.3f);
    }

    if (damage_factor > 0.0f) {
        /* Apply damage to hair cells - OHC more vulnerable than IHC */
        for (uint32_t i = 0; i < bridge->health.num_channels; i++) {
            /* Higher frequencies more vulnerable */
            float freq_vulnerability = (float)i / (float)bridge->health.num_channels;
            float ohc_damage = damage_factor * (0.5f + 0.5f * freq_vulnerability);
            float ihc_damage = damage_factor * 0.3f * (0.5f + 0.5f * freq_vulnerability);

            bridge->health.ohc_survival[i] = immune_clampf(
                bridge->health.ohc_survival[i] - ohc_damage, 0.0f, 1.0f);
            bridge->health.ihc_survival[i] = immune_clampf(
                bridge->health.ihc_survival[i] - ihc_damage, 0.0f, 1.0f);
        }

        /* Increase oxidative stress */
        bridge->health.oxidative_stress = immune_clampf(
            bridge->health.oxidative_stress + damage_factor, 0.0f, 1.0f);

        /* Trigger immune response if enabled */
        if (bridge->config.enable_immune_response &&
            damage_factor > bridge->config.inflammation_threshold) {
            bridge->response.response_active = true;
            bridge->response.damage_timestamp = immune_get_time_ms();
            bridge->response.macrophage_activity = immune_clampf(damage_factor * 0.5f, 0.0f, 1.0f);
            bridge->response.neuroprotection = 0.2f;
            bridge->response.recovery_rate = bridge->config.recovery_rate_per_hour;
            bridge->recovery_progress = 0.0f;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_immune_bridge_heartbeat("apply_exposure", 1.0f);
    return 0;
}

nimcp_error_t cochlea_immune_apply_trauma(
    cochlea_immune_bridge_t* bridge,
    float peak_level_db,
    uint32_t affected_channel
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_apply_trauma: bridge NULL");
        return -1;
    }

    cochlea_immune_bridge_heartbeat("apply_trauma", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->health.damage_type = COCHLEA_DAMAGE_ACOUSTIC_TRAUMA;

    /* Apply severe localized damage */
    if (affected_channel < bridge->health.num_channels) {
        /* Damage spreads to adjacent channels */
        int start = (int)affected_channel - 3;
        int end = (int)affected_channel + 3;
        if (start < 0) start = 0;
        if (end >= (int)bridge->health.num_channels) end = (int)bridge->health.num_channels - 1;

        for (int c = start; c <= end; c++) {
            float dist = fabsf((float)(c - (int)affected_channel));
            float spread = 1.0f - (dist / 4.0f);
            if (spread < 0.0f) spread = 0.0f;

            float damage = spread * (peak_level_db / bridge->config.trauma_threshold_db) * 0.5f;
            bridge->health.ohc_survival[c] = immune_clampf(
                bridge->health.ohc_survival[c] - damage, 0.0f, 1.0f);
            bridge->health.ihc_survival[c] = immune_clampf(
                bridge->health.ihc_survival[c] - damage * 0.5f, 0.0f, 1.0f);
        }
    }

    /* Severe oxidative stress */
    bridge->health.oxidative_stress = immune_clampf(
        bridge->health.oxidative_stress + 0.5f, 0.0f, 1.0f);

    /* Trigger acute inflammation */
    bridge->health.inflammation = INFLAMMATION_ACUTE;
    bridge->health.cytokine_level = immune_clampf(
        bridge->health.cytokine_level + 0.4f, 0.0f, 1.0f);

    /* Trigger immune response */
    if (bridge->config.enable_immune_response) {
        bridge->response.response_active = true;
        bridge->response.damage_timestamp = immune_get_time_ms();
        bridge->response.macrophage_activity = 0.7f;
        bridge->response.neuroprotection = 0.3f;
        bridge->response.recovery_rate = bridge->config.recovery_rate_per_hour * 0.5f;
        bridge->recovery_progress = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_immune_bridge_heartbeat("apply_trauma", 1.0f);
    return 0;
}

//=============================================================================
// Immune Response
//=============================================================================

nimcp_error_t cochlea_immune_get_response(
    const cochlea_immune_bridge_t* bridge,
    immune_response_t* response
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_get_response: bridge NULL");
        return -1;
    }
    if (!response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_get_response: response NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *response = bridge->response;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

nimcp_error_t cochlea_immune_trigger_protection(
    cochlea_immune_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_trigger_protection: bridge NULL");
        return -1;
    }

    cochlea_immune_bridge_heartbeat("trigger_protection", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    /* Activate protective immune response */
    bridge->response.response_active = true;
    bridge->response.neuroprotection = immune_clampf(
        bridge->response.neuroprotection + 0.3f, 0.0f, 1.0f);
    bridge->response.recovery_rate = immune_clampf(
        bridge->response.recovery_rate + bridge->config.recovery_rate_per_hour * 0.5f,
        0.0f, 1.0f);

    /* Reduce oxidative stress through neuroprotection */
    bridge->health.oxidative_stress = immune_clampf(
        bridge->health.oxidative_stress - 0.1f, 0.0f, 1.0f);

    bridge->last_inbound_ts = immune_get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_immune_bridge_heartbeat("trigger_protection", 1.0f);
    return 0;
}

//=============================================================================
// Recovery
//=============================================================================

nimcp_error_t cochlea_immune_simulate_recovery(
    cochlea_immune_bridge_t* bridge,
    float hours
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_simulate_recovery: bridge NULL");
        return -1;
    }

    cochlea_immune_bridge_heartbeat("simulate_recovery", 0.0f);
    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->health.damage_type == COCHLEA_DAMAGE_NONE) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    float recovery_amount = hours * bridge->config.recovery_rate_per_hour;

    /* Only TTS damage recovers fully; PTS and trauma partially recover */
    float recovery_cap = 1.0f;
    if (bridge->health.damage_type == COCHLEA_DAMAGE_PERMANENT) {
        recovery_cap = 0.3f;  /* Only 30% recovery for PTS */
    } else if (bridge->health.damage_type == COCHLEA_DAMAGE_ACOUSTIC_TRAUMA) {
        recovery_cap = 0.5f;  /* 50% recovery for trauma */
    }

    recovery_amount = immune_clampf(recovery_amount, 0.0f, recovery_cap);

    /* Apply recovery to hair cells */
    for (uint32_t i = 0; i < bridge->health.num_channels; i++) {
        bridge->health.ohc_survival[i] = immune_clampf(
            bridge->health.ohc_survival[i] + recovery_amount, 0.0f, 1.0f);
        bridge->health.ihc_survival[i] = immune_clampf(
            bridge->health.ihc_survival[i] + recovery_amount, 0.0f, 1.0f);
    }

    /* Reduce oxidative stress */
    bridge->health.oxidative_stress = immune_clampf(
        bridge->health.oxidative_stress - recovery_amount * 0.5f, 0.0f, 1.0f);

    /* Reduce cytokine levels */
    bridge->health.cytokine_level = immune_clampf(
        bridge->health.cytokine_level - recovery_amount * 0.3f, 0.0f, 1.0f);

    /* Update inflammation */
    if (bridge->health.oxidative_stress < bridge->config.inflammation_threshold) {
        if (bridge->health.inflammation == INFLAMMATION_ACUTE) {
            bridge->health.inflammation = INFLAMMATION_RESOLVING;
        } else if (bridge->health.inflammation == INFLAMMATION_RESOLVING) {
            bridge->health.inflammation = INFLAMMATION_NONE;
        }
    }

    /* Update recovery progress */
    bridge->recovery_progress = immune_clampf(
        bridge->recovery_progress + recovery_amount, 0.0f, 1.0f);

    /* If fully recovered, clear damage type */
    if (bridge->recovery_progress >= 1.0f &&
        bridge->health.damage_type == COCHLEA_DAMAGE_TEMPORARY) {
        bridge->health.damage_type = COCHLEA_DAMAGE_NONE;
        bridge->response.response_active = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_immune_bridge_heartbeat("simulate_recovery", 1.0f);
    return 0;
}

float cochlea_immune_get_recovery_progress(
    const cochlea_immune_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_get_recovery_progress: bridge NULL");
        return 0.0f;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    float p = bridge->recovery_progress;
    nimcp_mutex_unlock(bridge->base.mutex);
    return p;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_immune_verify_bidirectional(const cochlea_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_verify_bidirectional: bridge NULL");
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool ok = (bridge->last_outbound_ts > 0) && (bridge->last_inbound_ts > 0);
    nimcp_mutex_unlock(bridge->base.mutex);
    return ok;
}

uint64_t cochlea_immune_get_last_outbound(const cochlea_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_get_last_outbound: bridge NULL");
        return 0;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_outbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}

uint64_t cochlea_immune_get_last_inbound(const cochlea_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_immune_get_last_inbound: bridge NULL");
        return 0;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_inbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}
