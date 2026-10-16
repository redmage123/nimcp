/**
 * @file nimcp_cochlea_kg_bridge.c
 * @brief Cochlea-Knowledge Graph integration implementation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_kg_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_kg_bridge)

#define LOG_MODULE "COCHLEA_KG_BRIDGE"

//=============================================================================
// Internal Structure
//=============================================================================

struct cochlea_kg_bridge {
    bridge_base_t base;                     /**< MUST be first */
    cochlea_kg_config_t config;

    /* Connected systems */
    cochlea_t* cochlea;
    kg_engine_t* kg;

    /* Signature storage */
    auditory_signature_t signatures[COCHLEA_KG_MAX_SIGNATURES];
    uint32_t num_signatures;

    /* Self-awareness model */
    cochlea_kg_self_t self_model;
    bool self_registered;

    /* Recognition state */
    recognition_result_t last_recognition;

    /* Timing */
    float time_since_update_ms;

    /* Bidirectional timestamps */
    uint64_t last_outbound_ms;
    uint64_t last_inbound_ms;

    /* Node ID counter (stub: simulated KG) */
    kg_node_id_t next_node_id;
};

//=============================================================================
// Helper: Get current time in milliseconds
//=============================================================================

static uint64_t kg_bridge_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Helper: Allocate node ID (stub)
//=============================================================================

static kg_node_id_t allocate_node_id(cochlea_kg_bridge_t* bridge) {
    return ++(bridge->next_node_id);
}

//=============================================================================
// Configuration
//=============================================================================

cochlea_kg_config_t cochlea_kg_config_default(void) {
    cochlea_kg_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.cochlea_name = "cochlea_primary";
    cfg.register_on_create = true;
    cfg.enable_recognition = true;
    cfg.recognition_threshold = 0.7f;
    cfg.max_signatures = COCHLEA_KG_MAX_SIGNATURES;
    cfg.enable_self_awareness = true;
    cfg.update_interval_ms = 1000.0f;

    return cfg;
}

//=============================================================================
// Core API
//=============================================================================

cochlea_kg_bridge_t* cochlea_kg_bridge_create(
    cochlea_t* cochlea,
    kg_engine_t* kg,
    const cochlea_kg_config_t* config)
{
    cochlea_kg_bridge_heartbeat("create", 0.0f);

    cochlea_kg_bridge_t* bridge =
        (cochlea_kg_bridge_t*)nimcp_calloc(1, sizeof(cochlea_kg_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "cochlea_kg_bridge_create: alloc failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cochlea_kg_config_default();
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_kg_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_kg_bridge_create: validation failed");
        return NULL;
    }

    bridge->cochlea = cochlea;
    bridge->kg = kg;
    bridge->num_signatures = 0;
    bridge->self_registered = false;
    bridge->time_since_update_ms = 0.0f;
    bridge->next_node_id = 100;  /* Start node IDs at 100 */

    /* Connect systems via base */
    if (cochlea) {
        bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    }
    if (kg) {
        bridge_base_connect_b_unlocked(&bridge->base, kg);
    }

    /* Auto-register self in KG if configured */
    if (bridge->config.register_on_create && bridge->config.enable_self_awareness) {
        bridge->self_model.cochlea_node = allocate_node_id(bridge);
        bridge->self_model.health_node = allocate_node_id(bridge);
        bridge->self_model.capability_node = allocate_node_id(bridge);
        bridge->self_model.current_health = 1.0f;
        bridge->self_model.num_channels = 128;
        bridge->self_model.freq_range_low_hz = 20.0f;
        bridge->self_model.freq_range_high_hz = 20000.0f;
        bridge->self_registered = true;
    }

    cochlea_kg_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_kg_bridge_destroy(cochlea_kg_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_kg");
    cochlea_kg_bridge_heartbeat("destroy", 0.0f);

    /* Free dynamically allocated signature templates */
    for (uint32_t i = 0; i < bridge->num_signatures; i++) {
        if (bridge->signatures[i].spectral_template) {
            nimcp_free(bridge->signatures[i].spectral_template);
        }
        if (bridge->signatures[i].temporal_template) {
            nimcp_free(bridge->signatures[i].temporal_template);
        }
    }

    /* Free recognition related nodes if allocated */
    if (bridge->last_recognition.related_nodes) {
        nimcp_free(bridge->last_recognition.related_nodes);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_kg_bridge_update(
    cochlea_kg_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_bridge_update: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_bridge_update: cochlea_output NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_kg_bridge_heartbeat("update", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->time_since_update_ms += dt_ms;

    /* Periodically update self-awareness in KG */
    if (bridge->config.enable_self_awareness &&
        bridge->time_since_update_ms >= bridge->config.update_interval_ms) {
        bridge->self_model.current_health = 1.0f;  /* Stub: always healthy */
        bridge->time_since_update_ms = 0.0f;
        bridge->last_outbound_ms = kg_bridge_time_ms();
    }

    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_kg_bridge_heartbeat("update", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_kg_bridge_reset(cochlea_kg_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_bridge_reset: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_kg_bridge_heartbeat("reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Free signature templates */
    for (uint32_t i = 0; i < bridge->num_signatures; i++) {
        if (bridge->signatures[i].spectral_template) {
            nimcp_free(bridge->signatures[i].spectral_template);
            bridge->signatures[i].spectral_template = NULL;
        }
        if (bridge->signatures[i].temporal_template) {
            nimcp_free(bridge->signatures[i].temporal_template);
            bridge->signatures[i].temporal_template = NULL;
        }
    }
    bridge->num_signatures = 0;

    /* Free recognition result nodes */
    if (bridge->last_recognition.related_nodes) {
        nimcp_free(bridge->last_recognition.related_nodes);
        bridge->last_recognition.related_nodes = NULL;
    }
    memset(&bridge->last_recognition, 0, sizeof(recognition_result_t));

    bridge->time_since_update_ms = 0.0f;
    bridge->last_outbound_ms = 0;
    bridge->last_inbound_ms = 0;

    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_kg_bridge_heartbeat("reset", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Signature Management
//=============================================================================

nimcp_error_t cochlea_kg_add_signature(
    cochlea_kg_bridge_t* bridge,
    const char* label,
    const float* spectral_features,
    const float* temporal_features,
    uint32_t feature_dim)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_add_signature: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!label) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_add_signature: label NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!spectral_features || !temporal_features || feature_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_add_signature: features NULL or dim 0");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_kg_bridge_heartbeat("add_signature", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_signatures >= bridge->config.max_signatures ||
        bridge->num_signatures >= COCHLEA_KG_MAX_SIGNATURES) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    uint32_t dim = (feature_dim > COCHLEA_KG_FEATURE_DIM) ? COCHLEA_KG_FEATURE_DIM : feature_dim;
    uint32_t idx = bridge->num_signatures;
    auditory_signature_t* sig = &bridge->signatures[idx];
    memset(sig, 0, sizeof(auditory_signature_t));

    sig->node_id = allocate_node_id(bridge);
    strncpy(sig->label, label, sizeof(sig->label) - 1);
    sig->label[sizeof(sig->label) - 1] = '\0';
    sig->feature_dim = dim;
    sig->confidence = 0.0f;
    sig->last_heard_ms = 0;
    sig->recognition_count = 0;

    /* Allocate and copy spectral template */
    sig->spectral_template = (float*)nimcp_calloc(dim, sizeof(float));
    if (!sig->spectral_template) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "cochlea_kg_add_signature: spectral alloc failed");
        return NIMCP_ERROR_NO_MEMORY;
    }
    memcpy(sig->spectral_template, spectral_features, dim * sizeof(float));

    /* Allocate and copy temporal template */
    sig->temporal_template = (float*)nimcp_calloc(dim, sizeof(float));
    if (!sig->temporal_template) {
        nimcp_free(sig->spectral_template);
        sig->spectral_template = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "cochlea_kg_add_signature: temporal alloc failed");
        return NIMCP_ERROR_NO_MEMORY;
    }
    memcpy(sig->temporal_template, temporal_features, dim * sizeof(float));

    bridge->num_signatures++;

    /* Outbound: we stored a new concept in the KG */
    bridge->last_outbound_ms = kg_bridge_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_kg_bridge_heartbeat("add_signature", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_kg_remove_signature(
    cochlea_kg_bridge_t* bridge,
    kg_node_id_t node_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_remove_signature: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_kg_bridge_heartbeat("remove_signature", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    int found = -1;
    for (uint32_t i = 0; i < bridge->num_signatures; i++) {
        if (bridge->signatures[i].node_id == node_id) {
            found = (int)i;
            break;
        }
    }

    if (found < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Free templates */
    if (bridge->signatures[found].spectral_template) {
        nimcp_free(bridge->signatures[found].spectral_template);
    }
    if (bridge->signatures[found].temporal_template) {
        nimcp_free(bridge->signatures[found].temporal_template);
    }

    /* Shift remaining signatures down */
    for (uint32_t i = (uint32_t)found; i < bridge->num_signatures - 1; i++) {
        bridge->signatures[i] = bridge->signatures[i + 1];
    }
    bridge->num_signatures--;

    /* Clear last slot */
    memset(&bridge->signatures[bridge->num_signatures], 0, sizeof(auditory_signature_t));

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_kg_get_signature(
    const cochlea_kg_bridge_t* bridge,
    kg_node_id_t node_id,
    auditory_signature_t* signature)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_get_signature: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_get_signature: signature NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->num_signatures; i++) {
        if (bridge->signatures[i].node_id == node_id) {
            *signature = bridge->signatures[i];
            /* Note: shallow copy -- pointers shared. Caller must not free. */
            nimcp_mutex_unlock(bridge->base.mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_ERROR_NOT_FOUND;
}

//=============================================================================
// Recognition
//=============================================================================

nimcp_error_t cochlea_kg_recognize(
    cochlea_kg_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    recognition_result_t* result)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_recognize: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_recognize: cochlea_output NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_recognize: result NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_kg_bridge_heartbeat("recognize", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Initialize result to no match */
    memset(result, 0, sizeof(recognition_result_t));
    result->recognized = false;
    result->match_confidence = 0.0f;
    result->related_nodes = NULL;
    result->num_related = 0;

    if (!bridge->config.enable_recognition || bridge->num_signatures == 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        cochlea_kg_bridge_heartbeat("recognize", 1.0f);
        return NIMCP_SUCCESS;
    }

    /* Stub recognition: find best match by simulated confidence */
    float best_conf = 0.0f;
    int best_idx = -1;
    for (uint32_t i = 0; i < bridge->num_signatures; i++) {
        /* Stub: use a simulated confidence based on recognition count */
        float conf = 0.5f + 0.01f * (float)bridge->signatures[i].recognition_count;
        if (conf > 1.0f) conf = 1.0f;
        if (conf > best_conf) {
            best_conf = conf;
            best_idx = (int)i;
        }
    }

    if (best_idx >= 0 && best_conf >= bridge->config.recognition_threshold) {
        result->recognized = true;
        result->matched_node = bridge->signatures[best_idx].node_id;
        strncpy(result->matched_label, bridge->signatures[best_idx].label,
                sizeof(result->matched_label) - 1);
        result->match_confidence = best_conf;

        bridge->signatures[best_idx].recognition_count++;
        bridge->signatures[best_idx].last_heard_ms = kg_bridge_time_ms();
        bridge->signatures[best_idx].confidence = best_conf;
    }

    /* Record inbound: KG returned recognition result */
    bridge->last_inbound_ms = kg_bridge_time_ms();
    bridge->last_recognition = *result;
    bridge->last_recognition.related_nodes = NULL;  /* Don't share pointer */

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_kg_bridge_heartbeat("recognize", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_kg_learn_current(
    cochlea_kg_bridge_t* bridge,
    const char* label)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_learn_current: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!label) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_learn_current: label NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_kg_bridge_heartbeat("learn_current", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_signatures >= bridge->config.max_signatures ||
        bridge->num_signatures >= COCHLEA_KG_MAX_SIGNATURES) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Create a stub signature from "current" audio (simulated) */
    uint32_t idx = bridge->num_signatures;
    auditory_signature_t* sig = &bridge->signatures[idx];
    memset(sig, 0, sizeof(auditory_signature_t));

    sig->node_id = allocate_node_id(bridge);
    strncpy(sig->label, label, sizeof(sig->label) - 1);
    sig->label[sizeof(sig->label) - 1] = '\0';
    sig->feature_dim = COCHLEA_KG_FEATURE_DIM;
    sig->confidence = 0.0f;
    sig->last_heard_ms = kg_bridge_time_ms();
    sig->recognition_count = 0;

    /* Allocate zero-initialized templates (to be filled by real audio later) */
    sig->spectral_template = (float*)nimcp_calloc(COCHLEA_KG_FEATURE_DIM, sizeof(float));
    sig->temporal_template = (float*)nimcp_calloc(COCHLEA_KG_FEATURE_DIM, sizeof(float));

    if (!sig->spectral_template || !sig->temporal_template) {
        if (sig->spectral_template) nimcp_free(sig->spectral_template);
        if (sig->temporal_template) nimcp_free(sig->temporal_template);
        sig->spectral_template = NULL;
        sig->temporal_template = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "cochlea_kg_learn_current: template alloc failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    bridge->num_signatures++;

    bridge->last_outbound_ms = kg_bridge_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Self-Awareness
//=============================================================================

nimcp_error_t cochlea_kg_get_self(
    const cochlea_kg_bridge_t* bridge,
    cochlea_kg_self_t* self)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_get_self: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!self) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_get_self: self NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *self = bridge->self_model;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_kg_update_self(cochlea_kg_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_update_self: bridge NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_kg_bridge_heartbeat("update_self", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update self model with current state */
    bridge->self_model.current_health = 1.0f;
    bridge->self_model.num_channels = 128;
    bridge->self_model.freq_range_low_hz = 20.0f;
    bridge->self_model.freq_range_high_hz = 20000.0f;

    bridge->last_outbound_ms = kg_bridge_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

kg_node_id_t cochlea_kg_get_node(const cochlea_kg_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_get_node: bridge NULL");
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    kg_node_id_t node = bridge->self_model.cochlea_node;
    nimcp_mutex_unlock(bridge->base.mutex);

    return node;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_kg_verify_bidirectional(const cochlea_kg_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_verify_bidirectional: bridge NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool bidir = (bridge->last_outbound_ms > 0) && (bridge->last_inbound_ms > 0);
    nimcp_mutex_unlock(bridge->base.mutex);

    return bidir;
}

uint64_t cochlea_kg_get_last_outbound(const cochlea_kg_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_get_last_outbound: bridge NULL");
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_outbound_ms;
    nimcp_mutex_unlock(bridge->base.mutex);

    return ts;
}

uint64_t cochlea_kg_get_last_inbound(const cochlea_kg_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cochlea_kg_get_last_inbound: bridge NULL");
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_inbound_ms;
    nimcp_mutex_unlock(bridge->base.mutex);

    return ts;
}
