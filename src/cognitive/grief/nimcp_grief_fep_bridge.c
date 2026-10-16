/**
 * @file nimcp_grief_fep_bridge.c
 * @brief Free Energy Principle - Grief/Loss Integration Bridge Implementation
 *
 * WHAT: Implements bidirectional integration between FEP and grief/loss system
 * WHY:  Grief is processing prediction errors from permanent loss - the world model
 *       must update to reflect the absence of an attachment figure
 * HOW:  FEP drives grief intensity via persistent prediction errors; grief modulates
 *       learning rates and model updating
 */

#include "cognitive/grief/nimcp_grief_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "grief_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(grief_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from grief_fep_bridge module (instance-level) */
static inline void grief_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_grief_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_grief_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_grief_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int grief_fep_default_config(grief_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_default_co", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* FEP -> Grief */
    config->pe_grief_intensity_gain = 1.0f;
    config->persistent_pe_duration_threshold = 1000.0f;  /* 1 second threshold */
    config->enable_pe_grief_generation = true;
    config->enable_persistent_pe_detection = true;

    /* Grief -> FEP */
    config->grief_learning_rate_reduction = 0.5f;  /* Grief slows learning */
    config->emotional_pain_precision_gain = 1.0f;
    config->enable_grief_learning_slowdown = true;
    config->enable_pain_precision = true;

    /* Sensitivity */
    config->fe_sensitivity = 1.0f;
    config->emotion_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

grief_fep_bridge_t* grief_fep_create(
    const grief_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_create", 0.0f);


    grief_fep_bridge_t* bridge = nimcp_malloc(sizeof(grief_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate grief FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(grief_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        grief_fep_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "grief_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "grief_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize defaults */
    bridge->emotion_effects.learning_rate_modifier = 1.0f;
    bridge->emotion_effects.model_update_resistance = 0.0f;

    NIMCP_LOGGING_INFO("Created grief FEP bridge");
    return bridge;
}

void grief_fep_destroy(grief_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        grief_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed grief FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int grief_fep_connect_fep(
    grief_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_connect_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to grief bridge");
    return 0;
}

int grief_fep_connect_grief(
    grief_fep_bridge_t* bridge,
    grief_system_t* grief
) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_connect_gr", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && grief, NIMCP_ERROR_NULL_POINTER, "bridge or grief is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->grief_system = grief;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected grief system to FEP bridge");
    return 0;
}

int grief_fep_disconnect(grief_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->grief_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected all systems from grief FEP bridge");
    return 0;
}

/* ============================================================================
 * FEP -> Grief Direction
 * ============================================================================ */

int grief_fep_process_persistent_pe(
    grief_fep_bridge_t* bridge,
    float pe_magnitude,
    uint64_t duration_ms
) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_process_pe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_pe_grief_generation) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Grief = massive, persistent prediction error
     * The brain expects the person but they are absent
     * PE magnitude represents the "surprise" of absence
     * Duration represents how long the absence has persisted
     */
    float grief_intensity = fabsf(pe_magnitude) * bridge->config.pe_grief_intensity_gain;

    /* Scale by duration - prolonged PE increases grief */
    float duration_factor = (float)duration_ms / bridge->config.persistent_pe_duration_threshold;
    if (duration_factor > 2.0f) duration_factor = 2.0f;  /* Cap at 2x */
    grief_intensity *= (0.5f + 0.5f * duration_factor);

    /* Clamp to [0, 1] */
    if (grief_intensity > 1.0f) grief_intensity = 1.0f;

    bridge->fep_effects.grief_intensity_from_pe = grief_intensity;

    /* Emotional pain scales with grief intensity */
    float emotional_pain = grief_intensity * bridge->config.emotional_pain_precision_gain;
    bridge->fep_effects.emotional_pain_level = emotional_pain;

    /* Detect persistent PE (indication of unresolved grief) */
    if (duration_ms > bridge->config.persistent_pe_duration_threshold && pe_magnitude > 0.5f) {
        bridge->fep_effects.persistent_pe_detected = true;
        bridge->stats.persistent_pe_events++;
        NIMCP_LOGGING_INFO("Persistent PE detected: duration=%lums, magnitude=%f",
                          (unsigned long)duration_ms, pe_magnitude);
    } else {
        bridge->fep_effects.persistent_pe_detected = false;
    }

    /* Update state */
    bridge->state.current_prediction_error = pe_magnitude;
    bridge->state.grief_intensity = grief_intensity;
    bridge->state.emotional_pain = emotional_pain;

    if (grief_intensity > 0.3f && !bridge->state.grieving) {
        bridge->state.grieving = true;
        bridge->state.grief_onset_time = duration_ms;
        bridge->stats.grief_episodes++;
        NIMCP_LOGGING_INFO("Grief episode started: intensity=%f", grief_intensity);
    } else if (grief_intensity < 0.1f && bridge->state.grieving) {
        bridge->state.grieving = false;
        NIMCP_LOGGING_INFO("Grief episode ended");
    }

    /* Update stats */
    bridge->stats.avg_grief_intensity =
        (bridge->stats.avg_grief_intensity * 0.9f) + (grief_intensity * 0.1f);
    bridge->stats.avg_emotional_pain =
        (bridge->stats.avg_emotional_pain * 0.9f) + (emotional_pain * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Processed persistent PE: grief=%f, pain=%f, duration=%lums",
                        grief_intensity, emotional_pain, (unsigned long)duration_ms);
    return 0;
}

/**
 * @brief Internal unlocked version of learning rate modulation.
 * Caller MUST hold bridge->base.mutex.
 */
static int grief_fep_modulate_learning_rate_unlocked(grief_fep_bridge_t* bridge) {
    if (!bridge->config.enable_grief_learning_slowdown) return 0;

    /* Grief slows learning rate
     * This models the biological phenomenon where grief suppresses
     * new learning to preserve memories of the lost person
     */
    float grief_intensity = bridge->state.grief_intensity;
    float learning_modifier = 1.0f - (grief_intensity * bridge->config.grief_learning_rate_reduction);

    /* Clamp to minimum learning */
    if (learning_modifier < 0.1f) learning_modifier = 0.1f;

    bridge->emotion_effects.learning_rate_modifier = learning_modifier;

    /* Model update resistance increases with grief
     * The brain resists updating its model to accept the loss
     */
    bridge->emotion_effects.model_update_resistance = grief_intensity * 0.8f;

    /* Pain precision weight - pain demands attention */
    float pain_precision = 1.0f + (bridge->state.emotional_pain *
                                   bridge->config.emotional_pain_precision_gain);
    bridge->emotion_effects.pain_precision_weight = pain_precision;

    NIMCP_LOGGING_DEBUG("Modulated learning rate: modifier=%f, resistance=%f",
                        learning_modifier, bridge->emotion_effects.model_update_resistance);
    return 0;
}

int grief_fep_modulate_learning_rate(
    grief_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_modulate_l", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    int rc = grief_fep_modulate_learning_rate_unlocked(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return rc;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int grief_fep_update(
    grief_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Apply learning rate modulation (unlocked — we already hold the mutex) */
    grief_fep_modulate_learning_rate_unlocked(bridge);

    /* Grief naturally decays over time (healthy grief resolution)
     * But decay is slow - grief persists
     */
    float decay = 0.9999f;  /* Very slow decay */
    bridge->state.grief_intensity *= decay;
    bridge->state.emotional_pain *= decay;

    if (bridge->state.grief_intensity < 0.01f) {
        bridge->state.grieving = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int grief_fep_get_state(
    grief_fep_bridge_t* bridge,
    grief_fep_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int grief_fep_get_stats(
    grief_fep_bridge_t* bridge,
    grief_fep_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int grief_fep_connect_bio_async(
    grief_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_connect_bi", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_GRIEF_BRIDGE,
        .module_name = "grief_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available");
    }

    return 0;
}

int grief_fep_disconnect_bio_async(
    grief_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool grief_fep_is_bio_async_connected(
    const grief_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_grief_fep_is_bio_asy", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int grief_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    grief_fep_bridge_heartbeat("grief_fep_br_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Grief_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                grief_fep_bridge_heartbeat("grief_fep_br_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Grief_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Grief_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Instance-Level Health Agent (Phase 8 Utility Integration)
 * ============================================================================ */

static nimcp_health_agent_t* g_grief_fep_bridge_instance_health_agent = NULL;

void grief_fep_bridge_set_instance_health_agent(grief_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    (void)bridge;
    g_grief_fep_bridge_instance_health_agent = agent;
}

/* ============================================================================
 * Training Stubs (Phase 8 Utility Integration)
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */

int grief_fep_bridge_training_begin(grief_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "grief_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    grief_fep_bridge_heartbeat_instance(g_grief_fep_bridge_instance_health_agent, "grief_fep_bridge_training_begin", 0.0f);
    return 0;
}

int grief_fep_bridge_training_end(grief_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "grief_fep_bridge_training_end: NULL argument");
        return -1;
    }
    grief_fep_bridge_heartbeat_instance(g_grief_fep_bridge_instance_health_agent, "grief_fep_bridge_training_end", 1.0f);
    return 0;
}

int grief_fep_bridge_training_step(grief_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "grief_fep_bridge_training_step: NULL argument");
        return -1;
    }
    grief_fep_bridge_heartbeat_instance(g_grief_fep_bridge_instance_health_agent, "grief_fep_bridge_training_step", progress);
    return 0;
}
