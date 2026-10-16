/**
 * @file nimcp_joy_fep_bridge.c
 * @brief Free Energy Principle - Joy/Euphoria Integration Bridge Implementation
 *
 * WHAT: Implements bidirectional integration between FEP and joy/euphoria system
 * WHY:  Joy arises from positive prediction errors (reward prediction errors) -
 *       better-than-expected outcomes generate positive affect
 * HOW:  FEP positive PEs trigger joy; joy modulates dopamine-mediated learning
 */

#include "cognitive/joy/nimcp_joy_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "joy_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(joy_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from joy_fep_bridge module (instance-level) */
static inline void joy_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_joy_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_joy_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_joy_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int joy_fep_default_config(joy_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_default_conf", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* FEP -> Joy */
    config->positive_pe_joy_gain = 1.0f;
    config->euphoria_threshold = 0.8f;  /* High positive PE triggers euphoria */
    config->enable_positive_pe_joy = true;
    config->enable_euphoria_detection = true;

    /* Joy -> FEP */
    config->joy_learning_rate_boost = 1.5f;  /* Joy enhances learning */
    config->joy_dopamine_modulation = 1.0f;
    config->enable_joy_learning_boost = true;
    config->enable_joy_dopamine = true;

    /* Sensitivity */
    config->fe_sensitivity = 1.0f;
    config->emotion_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

joy_fep_bridge_t* joy_fep_create(
    const joy_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_create", 0.0f);


    joy_fep_bridge_t* bridge = nimcp_malloc(sizeof(joy_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate joy FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(joy_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        joy_fep_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "joy_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "joy_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize defaults */
    bridge->emotion_effects.learning_rate_boost = 1.0f;
    bridge->emotion_effects.dopamine_multiplier = 1.0f;
    bridge->emotion_effects.value_update_gain = 1.0f;

    NIMCP_LOGGING_INFO("Created joy FEP bridge");
    return bridge;
}

void joy_fep_destroy(joy_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        joy_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed joy FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int joy_fep_connect_fep(
    joy_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to joy bridge");
    return 0;
}

int joy_fep_connect_joy(
    joy_fep_bridge_t* bridge,
    joy_system_t* joy
) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_connect_joy", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && joy, NIMCP_ERROR_NULL_POINTER, "bridge or joy is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->joy_system = joy;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected joy system to FEP bridge");
    return 0;
}

int joy_fep_disconnect(joy_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->joy_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected all systems from joy FEP bridge");
    return 0;
}

/* ============================================================================
 * FEP -> Joy Direction
 * ============================================================================ */

int joy_fep_process_positive_pe(
    joy_fep_bridge_t* bridge,
    float pe_magnitude
) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_process_posi", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_positive_pe_joy) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Positive prediction error = better than expected = joy
     * PE > 0: outcome better than predicted -> positive affect
     * The magnitude determines intensity of joy
     */
    float joy_intensity = 0.0f;
    float positive_valence = 0.0f;
    bool euphoria = false;

    if (pe_magnitude > 0.0f) {
        /* Positive PE generates joy */
        joy_intensity = pe_magnitude * bridge->config.positive_pe_joy_gain;
        if (joy_intensity > 1.0f) joy_intensity = 1.0f;

        positive_valence = joy_intensity;

        /* Check for euphoria threshold */
        if (joy_intensity > bridge->config.euphoria_threshold) {
            euphoria = true;
            bridge->stats.euphoria_events++;
            NIMCP_LOGGING_INFO("Euphoria triggered: joy_intensity=%f", joy_intensity);
        }
    }

    bridge->fep_effects.joy_intensity_from_pe = joy_intensity;
    bridge->fep_effects.positive_valence = positive_valence;
    bridge->fep_effects.euphoria_triggered = euphoria;

    /* Update state */
    bridge->state.current_positive_pe = pe_magnitude > 0.0f ? pe_magnitude : 0.0f;
    bridge->state.joy_intensity = joy_intensity;
    bridge->state.euphoria_intensity = euphoria ? joy_intensity : 0.0f;
    bridge->state.joyful = (joy_intensity > 0.3f);
    bridge->state.euphoric = euphoria;

    /* Update stats */
    if (joy_intensity > 0.1f) {
        bridge->stats.joy_events++;
    }
    bridge->stats.avg_joy_intensity =
        (bridge->stats.avg_joy_intensity * 0.9f) + (joy_intensity * 0.1f);
    bridge->stats.avg_positive_pe =
        (bridge->stats.avg_positive_pe * 0.9f) + (bridge->state.current_positive_pe * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Processed positive PE: joy=%f, valence=%f, euphoria=%s",
                        joy_intensity, positive_valence, euphoria ? "yes" : "no");
    return 0;
}

/**
 * @brief Internal unlocked version of learning rate boost.
 * Caller MUST hold bridge->base.mutex.
 */
static int joy_fep_boost_learning_rate_unlocked(joy_fep_bridge_t* bridge) {
    if (!bridge->config.enable_joy_learning_boost) return 0;

    /* Joy enhances learning rate
     * Positive affect facilitates encoding of successful behaviors
     * Dopamine release from positive RPE strengthens synaptic connections
     */
    float joy_intensity = bridge->state.joy_intensity;
    float learning_boost = 1.0f + (joy_intensity * (bridge->config.joy_learning_rate_boost - 1.0f));

    bridge->emotion_effects.learning_rate_boost = learning_boost;

    /* Dopamine multiplier */
    float dopamine = 1.0f + (joy_intensity * bridge->config.joy_dopamine_modulation);
    bridge->emotion_effects.dopamine_multiplier = dopamine;

    /* Value update gain - joy increases value assigned to current action */
    float value_gain = 1.0f + (joy_intensity * 0.5f);
    bridge->emotion_effects.value_update_gain = value_gain;

    NIMCP_LOGGING_DEBUG("Boosted learning: rate=%f, dopamine=%f, value_gain=%f",
                        learning_boost, dopamine, value_gain);
    return 0;
}

int joy_fep_boost_learning_rate(
    joy_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_boost_learni", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    int rc = joy_fep_boost_learning_rate_unlocked(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return rc;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int joy_fep_update(
    joy_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Apply learning rate boost (unlocked — we already hold the mutex) */
    joy_fep_boost_learning_rate_unlocked(bridge);

    /* Joy decays relatively quickly compared to grief
     * This models the transient nature of positive affect
     */
    float decay = 0.995f;
    bridge->state.joy_intensity *= decay;
    bridge->state.euphoria_intensity *= decay;

    if (bridge->state.joy_intensity < 0.1f) {
        bridge->state.joyful = false;
    }
    if (bridge->state.euphoria_intensity < bridge->config.euphoria_threshold) {
        bridge->state.euphoric = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int joy_fep_get_state(
    joy_fep_bridge_t* bridge,
    joy_fep_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int joy_fep_get_stats(
    joy_fep_bridge_t* bridge,
    joy_fep_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int joy_fep_connect_bio_async(
    joy_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_connect_bio_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_JOY_BRIDGE,
        .module_name = "joy_fep_bridge",
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

int joy_fep_disconnect_bio_async(
    joy_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_disconnect_b", 0.0f);


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

bool joy_fep_is_bio_async_connected(
    const joy_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_joy_fep_is_bio_async", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int joy_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    joy_fep_bridge_heartbeat("joy_fep_brid_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Joy_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                joy_fep_bridge_heartbeat("joy_fep_brid_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Joy_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Joy_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Instance-Level Health Agent (Phase 8 Utility Integration)
 * ============================================================================ */

static nimcp_health_agent_t* g_joy_fep_bridge_instance_health_agent = NULL;

void joy_fep_bridge_set_instance_health_agent(joy_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    (void)bridge;
    g_joy_fep_bridge_instance_health_agent = agent;
}

/* ============================================================================
 * Training Stubs (Phase 8 Utility Integration)
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */

int joy_fep_bridge_training_begin(joy_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "joy_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    joy_fep_bridge_heartbeat_instance(g_joy_fep_bridge_instance_health_agent, "joy_fep_bridge_training_begin", 0.0f);
    return 0;
}

int joy_fep_bridge_training_end(joy_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "joy_fep_bridge_training_end: NULL argument");
        return -1;
    }
    joy_fep_bridge_heartbeat_instance(g_joy_fep_bridge_instance_health_agent, "joy_fep_bridge_training_end", 1.0f);
    return 0;
}

int joy_fep_bridge_training_step(joy_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "joy_fep_bridge_training_step: NULL argument");
        return -1;
    }
    joy_fep_bridge_heartbeat_instance(g_joy_fep_bridge_instance_health_agent, "joy_fep_bridge_training_step", progress);
    return 0;
}
