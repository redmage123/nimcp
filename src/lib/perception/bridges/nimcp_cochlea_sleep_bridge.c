/**
 * @file nimcp_cochlea_sleep_bridge.c
 * @brief Cochlea-Sleep integration implementation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_sleep_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_sleep_bridge)

#define LOG_MODULE "COCHLEA_SLEEP_BRIDGE"

//=============================================================================
// Internal Structure
//=============================================================================

#define NAME_TEMPLATE_MAX_DIM 256

struct cochlea_sleep_bridge {
    bridge_base_t base;                     /**< MUST be first */
    cochlea_sleep_config_t config;

    /* Connected systems */
    cochlea_t* cochlea;
    sleep_controller_t* sleep;

    /* Current state */
    cochlea_sleep_stage_t current_stage;
    sleep_modulation_t modulation;

    /* Arousal tracking */
    arousal_event_t last_arousal;
    bool arousal_pending;

    /* Name template for name-detection */
    float* name_template;
    uint32_t name_template_dim;

    /* Bidirectional timestamps */
    uint64_t last_outbound_ms;
    uint64_t last_inbound_ms;

    /* Accumulated time */
    float accumulated_time_ms;
};

//=============================================================================
// Helper: Get current time in milliseconds
//=============================================================================

static uint64_t sleep_bridge_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Helper: Compute modulation for a given stage
//=============================================================================

static void compute_modulation(const cochlea_sleep_config_t* cfg,
                               cochlea_sleep_stage_t stage,
                               sleep_modulation_t* mod)
{
    mod->stage = stage;
    switch (stage) {
        case COCHLEA_SLEEP_WAKE:
            mod->gating_factor = cfg->wake_gating;
            mod->arousal_threshold_db = cfg->wake_threshold_db;
            mod->sensitivity_factor = 1.0f;
            break;
        case COCHLEA_SLEEP_N1:
            mod->gating_factor = cfg->n1_gating;
            mod->arousal_threshold_db = cfg->n1_threshold_db;
            mod->sensitivity_factor = 0.85f;
            break;
        case COCHLEA_SLEEP_N2:
            mod->gating_factor = cfg->n2_gating;
            mod->arousal_threshold_db = cfg->n2_threshold_db;
            mod->sensitivity_factor = 0.6f;
            break;
        case COCHLEA_SLEEP_N3:
            mod->gating_factor = cfg->n3_gating;
            mod->arousal_threshold_db = cfg->n3_threshold_db;
            mod->sensitivity_factor = 0.3f;
            break;
        case COCHLEA_SLEEP_REM:
            mod->gating_factor = cfg->rem_gating;
            mod->arousal_threshold_db = cfg->rem_threshold_db;
            mod->sensitivity_factor = 0.7f;
            break;
        default:
            mod->gating_factor = cfg->wake_gating;
            mod->arousal_threshold_db = cfg->wake_threshold_db;
            mod->sensitivity_factor = 1.0f;
            break;
    }
}

//=============================================================================
// Configuration
//=============================================================================

cochlea_sleep_config_t cochlea_sleep_config_default(void) {
    cochlea_sleep_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Stage-specific gating */
    cfg.wake_gating = 1.0f;
    cfg.n1_gating = 0.8f;
    cfg.n2_gating = 0.5f;
    cfg.n3_gating = 0.2f;
    cfg.rem_gating = 0.6f;

    /* Arousal thresholds (dB SPL) */
    cfg.wake_threshold_db = 90.0f;
    cfg.n1_threshold_db = 70.0f;
    cfg.n2_threshold_db = 60.0f;
    cfg.n3_threshold_db = 80.0f;
    cfg.rem_threshold_db = 65.0f;

    /* Special sounds */
    cfg.enable_name_detection = true;
    cfg.enable_alarm_detection = true;
    cfg.name_threshold_reduction = 20.0f;

    return cfg;
}

//=============================================================================
// Core API
//=============================================================================

cochlea_sleep_bridge_t* cochlea_sleep_bridge_create(
    cochlea_t* cochlea,
    sleep_controller_t* sleep,
    const cochlea_sleep_config_t* config)
{
    cochlea_sleep_bridge_heartbeat("create", 0.0f);

    cochlea_sleep_bridge_t* bridge =
        (cochlea_sleep_bridge_t*)nimcp_calloc(1, sizeof(cochlea_sleep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "cochlea_sleep_bridge_create: alloc failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cochlea_sleep_config_default();
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_sleep_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_sleep_bridge_create: validation failed");
        return NULL;
    }

    bridge->cochlea = cochlea;
    bridge->sleep = sleep;
    bridge->current_stage = COCHLEA_SLEEP_WAKE;
    bridge->arousal_pending = false;
    bridge->name_template = NULL;
    bridge->name_template_dim = 0;
    bridge->accumulated_time_ms = 0.0f;

    /* Connect systems via base */
    if (cochlea) {
        bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    }
    if (sleep) {
        bridge_base_connect_b_unlocked(&bridge->base, sleep);
    }

    /* Compute initial modulation for WAKE */
    compute_modulation(&bridge->config, bridge->current_stage, &bridge->modulation);

    cochlea_sleep_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_sleep_bridge_destroy(cochlea_sleep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_sleep");
    cochlea_sleep_bridge_heartbeat("destroy", 0.0f);

    if (bridge->name_template) {
        nimcp_free(bridge->name_template);
        bridge->name_template = NULL;
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_sleep_bridge_update(
    cochlea_sleep_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_bridge_update: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_bridge_update: cochlea_output NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_sleep_bridge_heartbeat("update", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->accumulated_time_ms += dt_ms;

    /* Recompute modulation for current stage */
    compute_modulation(&bridge->config, bridge->current_stage, &bridge->modulation);

    /* Record outbound timestamp */
    bridge->last_outbound_ms = sleep_bridge_time_ms();

    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_sleep_bridge_heartbeat("update", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_sleep_bridge_reset(cochlea_sleep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_bridge_reset: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_sleep_bridge_heartbeat("reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_stage = COCHLEA_SLEEP_WAKE;
    bridge->arousal_pending = false;
    memset(&bridge->last_arousal, 0, sizeof(arousal_event_t));
    bridge->accumulated_time_ms = 0.0f;
    bridge->last_outbound_ms = 0;
    bridge->last_inbound_ms = 0;

    compute_modulation(&bridge->config, bridge->current_stage, &bridge->modulation);

    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_sleep_bridge_heartbeat("reset", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Sleep Stage Control
//=============================================================================

nimcp_error_t cochlea_sleep_set_stage(
    cochlea_sleep_bridge_t* bridge,
    cochlea_sleep_stage_t stage)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_set_stage: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_sleep_bridge_heartbeat("set_stage", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_stage = stage;
    compute_modulation(&bridge->config, stage, &bridge->modulation);

    /* Inbound: sleep system tells cochlea about new stage */
    bridge->last_inbound_ms = sleep_bridge_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

cochlea_sleep_stage_t cochlea_sleep_get_stage(
    const cochlea_sleep_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_get_stage: bridge NULL");
        return COCHLEA_SLEEP_WAKE;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    cochlea_sleep_stage_t stage = bridge->current_stage;
    nimcp_mutex_unlock(bridge->base.mutex);

    return stage;
}

nimcp_error_t cochlea_sleep_get_modulation(
    const cochlea_sleep_bridge_t* bridge,
    sleep_modulation_t* modulation)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_get_modulation: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_get_modulation: modulation NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *modulation = bridge->modulation;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Arousal Detection
//=============================================================================

bool cochlea_sleep_check_arousal(
    const cochlea_sleep_bridge_t* bridge,
    arousal_event_t* event)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_check_arousal: bridge NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool pending = bridge->arousal_pending;
    if (pending && event) {
        *event = bridge->last_arousal;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return pending;
}

nimcp_error_t cochlea_sleep_clear_arousal(cochlea_sleep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_clear_arousal: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->arousal_pending = false;
    memset(&bridge->last_arousal, 0, sizeof(arousal_event_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Special Sound Detection
//=============================================================================

nimcp_error_t cochlea_sleep_set_name_template(
    cochlea_sleep_bridge_t* bridge,
    const float* template_features,
    uint32_t feature_dim)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_set_name_template: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!template_features || feature_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_set_name_template: template_features NULL or dim 0");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_sleep_bridge_heartbeat("set_name_template", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Free existing template */
    if (bridge->name_template) {
        nimcp_free(bridge->name_template);
        bridge->name_template = NULL;
    }

    uint32_t dim = (feature_dim > NAME_TEMPLATE_MAX_DIM) ? NAME_TEMPLATE_MAX_DIM : feature_dim;
    bridge->name_template = (float*)nimcp_calloc(dim, sizeof(float));
    if (!bridge->name_template) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "cochlea_sleep_set_name_template: alloc failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    memcpy(bridge->name_template, template_features, dim * sizeof(float));
    bridge->name_template_dim = dim;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_sleep_verify_bidirectional(const cochlea_sleep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_verify_bidirectional: bridge NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool bidir = (bridge->last_outbound_ms > 0) && (bridge->last_inbound_ms > 0);
    nimcp_mutex_unlock(bridge->base.mutex);

    return bidir;
}

uint64_t cochlea_sleep_get_last_outbound(const cochlea_sleep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_get_last_outbound: bridge NULL");
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_outbound_ms;
    nimcp_mutex_unlock(bridge->base.mutex);

    return ts;
}

uint64_t cochlea_sleep_get_last_inbound(const cochlea_sleep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_sleep_get_last_inbound: bridge NULL");
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_inbound_ms;
    nimcp_mutex_unlock(bridge->base.mutex);

    return ts;
}
