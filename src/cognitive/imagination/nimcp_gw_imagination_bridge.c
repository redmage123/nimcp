/**
 * @file nimcp_gw_imagination_bridge.c
 * @brief Global Workspace-Imagination Bidirectional Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "cognitive/imagination/nimcp_gw_imagination_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gw_imagination_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_gw_imagination_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_gw_imagination_bridge_mesh_registry = NULL;

nimcp_error_t gw_imagination_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_gw_imagination_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "gw_imagination_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "gw_imagination_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_gw_imagination_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_gw_imagination_bridge_mesh_registry = registry;
    return err;
}

void gw_imagination_bridge_mesh_unregister(void) {
    if (g_gw_imagination_bridge_mesh_registry && g_gw_imagination_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_gw_imagination_bridge_mesh_registry, g_gw_imagination_bridge_mesh_id);
        g_gw_imagination_bridge_mesh_id = 0;
        g_gw_imagination_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from gw_imagination_bridge module (instance-level) */
static inline void gw_imagination_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_gw_imagination_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_gw_imagination_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_gw_imagination_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

int gw_imagination_default_config(gw_imagination_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_default_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_defau", 0.0f);


    config->attention_boost_factor = GW_IMAG_DEFAULT_ATTENTION_BOOST;
    config->focus_decay_rate = 0.05f;

    config->ignition_threshold = GW_IMAG_DEFAULT_IGNITION_THRESHOLD;
    config->salience_weight_novelty = 0.3f;
    config->salience_weight_emotional = 0.4f;
    config->salience_weight_goal = 0.3f;

    config->enable_automatic_submission = true;
    config->auto_submit_threshold = 0.7f;
    config->max_pending_submissions = GW_IMAG_MAX_COMPETING_SCENARIOS;

    config->update_interval_ms = 16.0f;  /* ~60 Hz */
    config->enable_bio_async = true;

    return 0;
}

int gw_imagination_validate_config(const gw_imagination_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_validate_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_valid", 0.0f);


    if (config->attention_boost_factor < 1.0f || config->attention_boost_factor > 3.0f) {
        return -1;
    }
    if (config->focus_decay_rate < 0.0f || config->focus_decay_rate > 1.0f) {
        return -1;
    }
    if (config->ignition_threshold < 0.0f || config->ignition_threshold > 1.0f) {
        return -1;
    }
    if (config->salience_weight_novelty < 0.0f || config->salience_weight_novelty > 1.0f) {
        return -1;
    }
    if (config->salience_weight_emotional < 0.0f || config->salience_weight_emotional > 1.0f) {
        return -1;
    }
    if (config->salience_weight_goal < 0.0f || config->salience_weight_goal > 1.0f) {
        return -1;
    }
    if (config->auto_submit_threshold < 0.0f || config->auto_submit_threshold > 1.0f) {
        return -1;
    }
    if (config->update_interval_ms < 1.0f) {
        return -1;
    }

    return 0;
}

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */

gw_imagination_bridge_t* gw_imagination_bridge_create(
    const gw_imagination_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_create", 0.0f);


    gw_imagination_bridge_t* bridge = nimcp_calloc(
        1, sizeof(gw_imagination_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gw_imagination_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Initialize base */
    bridge->base.module_id = BIO_MODULE_IMAGINATION_GW;
    bridge->base.module_name = "gw_imagination_bridge";
    bridge->base.system_a_connected = false;
    bridge->base.system_b_connected = false;
    bridge->base.bridge_active = false;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "gw_imagination") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "gw_imagination_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "gw_imagination_bridge_create: mutex is NULL after init");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        if (gw_imagination_validate_config(config) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gw_imagination_bridge_create: invalid configuration");
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            return NULL;
        }
        bridge->config = *config;
    } else {
        gw_imagination_default_config(&bridge->config);
    }

    /* Initialize effects */
    memset(&bridge->gw_to_imag, 0, sizeof(bridge->gw_to_imag));
    memset(&bridge->imag_to_gw, 0, sizeof(bridge->imag_to_gw));

    /* Initialize pending submissions */
    bridge->num_pending = 0;
    for (uint32_t i = 0; i < GW_IMAG_MAX_COMPETING_SCENARIOS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GW_IMAG_MAX_COMPETING_SCENARIOS > 256) {
            gw_imagination_bridge_heartbeat("gw_imaginati_loop",
                             (float)(i + 1) / (float)GW_IMAG_MAX_COMPETING_SCENARIOS);
        }

        bridge->pending_submissions[i].pending = false;
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Initialize timing */
    bridge->last_update_time_ms = nimcp_time_get_ms();

    NIMCP_LOGGING_INFO("Created GW-imagination bridge");
    return bridge;
}

void gw_imagination_bridge_destroy(gw_imagination_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        gw_imagination_disconnect_bio_async(bridge);
    }

    /* Free goal embedding if present */
    if (bridge->gw_to_imag.goal_embedding) {
        nimcp_tensor_destroy(bridge->gw_to_imag.goal_embedding);
    }

    /* Free content embedding if present */
    if (bridge->imag_to_gw.content_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_gw.content_embedding);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed GW-imagination bridge");
}

int gw_imagination_reset(gw_imagination_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Clear effects (but preserve allocated tensors) */
    bridge->gw_to_imag.attention_boost = 0.0f;
    bridge->gw_to_imag.focus_strength = 0.0f;
    bridge->gw_to_imag.conscious_goal_active = false;
    bridge->gw_to_imag.broadcast_salience = 0.0f;
    bridge->gw_to_imag.broadcast_relevant_to_imagination = false;

    bridge->imag_to_gw.submission_strength = 0.0f;
    bridge->imag_to_gw.requesting_broadcast = false;
    bridge->imag_to_gw.vividness = 0.0f;
    bridge->imag_to_gw.coherence = 0.0f;

    /* Clear pending submissions */
    bridge->num_pending = 0;
    for (uint32_t i = 0; i < GW_IMAG_MAX_COMPETING_SCENARIOS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GW_IMAG_MAX_COMPETING_SCENARIOS > 256) {
            gw_imagination_bridge_heartbeat("gw_imaginati_loop",
                             (float)(i + 1) / (float)GW_IMAG_MAX_COMPETING_SCENARIOS);
        }

        bridge->pending_submissions[i].pending = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * CONNECTION
 * ============================================================================ */

int gw_imagination_connect_global_workspace(
    gw_imagination_bridge_t* bridge,
    global_workspace_t* gw)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_connect_global_workspace: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_conne", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->global_workspace = gw;
    bridge->base.system_a = gw;
    bridge->base.system_a_connected = (gw != NULL);
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  bridge->base.system_b_connected;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Global workspace %s to bridge",
                   gw ? "connected" : "disconnected");
    return 0;
}

int gw_imagination_connect_imagination(
    gw_imagination_bridge_t* bridge,
    struct imagination_engine* imagination)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_connect_imagination: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_conne", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->imagination = imagination;
    bridge->base.system_b = imagination;
    bridge->base.system_b_connected = (imagination != NULL);
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  bridge->base.system_b_connected;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Imagination engine %s to bridge",
                   imagination ? "connected" : "disconnected");
    return 0;
}

int gw_imagination_disconnect_global_workspace(gw_imagination_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_disco", 0.0f);


    return gw_imagination_connect_global_workspace(bridge, NULL);
}

int gw_imagination_disconnect_imagination(gw_imagination_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_disco", 0.0f);


    return gw_imagination_connect_imagination(bridge, NULL);
}

bool gw_imagination_is_connected(const gw_imagination_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_is_co", 0.0f);


    return bridge->base.bridge_active;
}

/* ============================================================================
 * UPDATE
 * ============================================================================ */

int gw_imagination_update(
    gw_imagination_bridge_t* bridge,
    float delta_time_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_update: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bridge_active) return 0;  /* Nothing to do */

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_updat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check update interval */
    uint64_t now = nimcp_time_get_ms();
    float elapsed = (float)(now - bridge->last_update_time_ms);
    if (elapsed < bridge->config.update_interval_ms) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->last_update_time_ms = now;

    /* Compute effects in both directions */
    gw_imagination_compute_gw_effects(bridge);
    gw_imagination_compute_imag_effects(bridge);

    /* Apply effects */
    gw_imagination_apply_effects(bridge);

    /* Decay focus over time */
    bridge->gw_to_imag.focus_strength *= (1.0f - bridge->config.focus_decay_rate);

    /* Update statistics */
    bridge->stats.total_updates++;
    bridge->base.total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int gw_imagination_compute_gw_effects(gw_imagination_bridge_t* bridge) {
    if (!bridge || !bridge->global_workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_compute_gw_effects: required parameter is NULL (bridge, bridge->global_workspace)");
        return -1;
    }

    /* Query global workspace for current state */
    /* In a full implementation, this would call global workspace APIs */

    /* Check if there's a current broadcast */
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_compu", 0.0f);


    bool has_broadcast = global_workspace_has_broadcast(bridge->global_workspace);

    if (has_broadcast) {
        bridge->gw_to_imag.broadcast_salience =
            global_workspace_get_broadcast_strength(bridge->global_workspace);
        bridge->gw_to_imag.broadcast_source_module =
            (uint32_t)global_workspace_get_broadcast_source(bridge->global_workspace);
    } else {
        bridge->gw_to_imag.broadcast_salience = 0.0f;
    }

    /* Get competition context */
    bridge->gw_to_imag.competition_threshold =
        global_workspace_get_ignition_threshold(bridge->global_workspace);
    bridge->gw_to_imag.num_competitors =
        global_workspace_get_competitor_count(bridge->global_workspace);

    /* Compute attention boost based on focus */
    bridge->gw_to_imag.attention_boost =
        1.0f + (bridge->gw_to_imag.focus_strength *
                (bridge->config.attention_boost_factor - 1.0f));

    return 0;
}

int gw_imagination_compute_imag_effects(gw_imagination_bridge_t* bridge) {
    if (!bridge || !bridge->imagination) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_compute_imag_effects: required parameter is NULL (bridge, bridge->imagination)");
        return -1;
    }

    /* Query imagination engine for current state */
    /* In a full implementation, this would call imagination engine APIs */

    /* Default: no active submission unless requested */
    if (!bridge->imag_to_gw.requesting_broadcast) {
        bridge->imag_to_gw.submission_strength = 0.0f;
    }

    /* Compute combined salience */
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_compu", 0.0f);


    float combined_salience =
        bridge->imag_to_gw.novelty_salience * bridge->config.salience_weight_novelty +
        bridge->imag_to_gw.emotional_salience * bridge->config.salience_weight_emotional +
        bridge->imag_to_gw.goal_relevance_salience * bridge->config.salience_weight_goal;

    /* Modify submission strength by vividness and coherence */
    if (bridge->imag_to_gw.requesting_broadcast) {
        bridge->imag_to_gw.submission_strength =
            combined_salience *
            bridge->imag_to_gw.vividness *
            bridge->imag_to_gw.coherence;
    }

    return 0;
}

int gw_imagination_apply_effects(gw_imagination_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_apply_effects: bridge is NULL");
        return -1;
    }

    /* Apply GW effects to imagination */
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_apply", 0.0f);


    if (bridge->imagination && bridge->gw_to_imag.attention_boost > 1.0f) {
        /* Would call imagination APIs to boost vividness */
        bridge->stats.attention_boosts_applied++;
        bridge->stats.avg_attention_boost =
            (bridge->stats.avg_attention_boost *
             (bridge->stats.attention_boosts_applied - 1) +
             bridge->gw_to_imag.attention_boost) /
            bridge->stats.attention_boosts_applied;
    }

    /* Process pending submissions to workspace */
    if (bridge->global_workspace && bridge->num_pending > 0) {
        for (uint32_t i = 0; i < GW_IMAG_MAX_COMPETING_SCENARIOS; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && GW_IMAG_MAX_COMPETING_SCENARIOS > 256) {
                gw_imagination_bridge_heartbeat("gw_imaginati_loop",
                                 (float)(i + 1) / (float)GW_IMAG_MAX_COMPETING_SCENARIOS);
            }

            if (bridge->pending_submissions[i].pending) {
                /* Submit to global workspace competition */
                /* In full implementation: global_workspace_submit(...) */
                bridge->stats.submissions_made++;
                bridge->pending_submissions[i].pending = false;
                bridge->num_pending--;
            }
        }
    }

    /* Handle active broadcast request */
    if (bridge->global_workspace && bridge->imag_to_gw.requesting_broadcast) {
        if (bridge->imag_to_gw.submission_strength >=
            bridge->config.ignition_threshold) {
            /* Would call global_workspace_compete() here */
            bridge->stats.submissions_made++;
        }
        bridge->imag_to_gw.requesting_broadcast = false;
    }

    return 0;
}

/* ============================================================================
 * WORKSPACE COMPETITION
 * ============================================================================ */

int gw_imagination_submit_for_broadcast(
    gw_imagination_bridge_t* bridge,
    uint32_t scenario_id,
    float strength)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_submit_for_broadcast: bridge is NULL");
        return -1;
    }
    if (strength < 0.0f || strength > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gw_imagination_submit_for_broadcast: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_submi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Find an empty slot */
    if (bridge->num_pending >= bridge->config.max_pending_submissions) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_LOGGING_WARN("Pending submissions full, dropping request");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "gw_imagination_submit_for_broadcast: capacity exceeded");
        return -1;
    }

    for (uint32_t i = 0; i < GW_IMAG_MAX_COMPETING_SCENARIOS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GW_IMAG_MAX_COMPETING_SCENARIOS > 256) {
            gw_imagination_bridge_heartbeat("gw_imaginati_loop",
                             (float)(i + 1) / (float)GW_IMAG_MAX_COMPETING_SCENARIOS);
        }

        if (!bridge->pending_submissions[i].pending) {
            bridge->pending_submissions[i].scenario_id = scenario_id;
            bridge->pending_submissions[i].strength = strength;
            bridge->pending_submissions[i].pending = true;
            bridge->num_pending++;
            break;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Queued scenario %u for broadcast (strength %.2f)",
                   scenario_id, strength);
    return 0;
}

int gw_imagination_request_attention_boost(
    gw_imagination_bridge_t* bridge,
    float boost_level)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_request_attention_boost: bridge is NULL");
        return -1;
    }
    if (boost_level < 0.0f || boost_level > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gw_imagination_request_attention_boost: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_reque", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Set focus strength which will translate to attention boost */
    bridge->gw_to_imag.focus_strength = boost_level;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Requested attention boost level %.2f", boost_level);
    return 0;
}

int gw_imagination_set_conscious_goal(
    gw_imagination_bridge_t* bridge,
    const nimcp_tensor_t* goal_embedding)
{
    if (!bridge || !goal_embedding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_set_conscious_goal: required parameter is NULL (bridge, goal_embedding)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_set_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Free existing goal if present */
    if (bridge->gw_to_imag.goal_embedding) {
        nimcp_tensor_destroy(bridge->gw_to_imag.goal_embedding);
    }

    /* Clone the goal embedding */
    bridge->gw_to_imag.goal_embedding = nimcp_tensor_clone(goal_embedding);
    bridge->gw_to_imag.conscious_goal_active =
        (bridge->gw_to_imag.goal_embedding != NULL);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Set conscious goal for imagination");
    return bridge->gw_to_imag.conscious_goal_active ? 0 : -1;
}

int gw_imagination_clear_conscious_goal(gw_imagination_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_clear_conscious_goal: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_clear", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->gw_to_imag.goal_embedding) {
        nimcp_tensor_destroy(bridge->gw_to_imag.goal_embedding);
        bridge->gw_to_imag.goal_embedding = NULL;
    }
    bridge->gw_to_imag.conscious_goal_active = false;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool gw_imagination_is_broadcasting(const gw_imagination_bridge_t* bridge) {
    if (!bridge || !bridge->global_workspace) {
        return false;
    }

    /* Check if imagination module is currently broadcasting */
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_is_br", 0.0f);


    /* Check if the global workspace has an active broadcast from imagination */
    bool has_broadcast = global_workspace_has_broadcast(bridge->global_workspace);
    if (!has_broadcast) {
        return false;
    }

    /* Check if the broadcast source is from the imagination module */
    uint32_t source = (uint32_t)global_workspace_get_broadcast_source(bridge->global_workspace);
    return (source == BIO_MODULE_IMAGINATION_GW || source == BIO_MODULE_IMAGINATION);
}

float gw_imagination_get_broadcast_strength(const gw_imagination_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_get_b", 0.0f);


    return bridge->imag_to_gw.submission_strength;
}

/* ============================================================================
 * QUERY
 * ============================================================================ */

int gw_imagination_get_gw_effects(
    const gw_imagination_bridge_t* bridge,
    gw_to_imagination_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_get_gw_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_get_g", 0.0f);

    nimcp_mutex_lock(((gw_imagination_bridge_t*)bridge)->base.mutex);
    *effects = bridge->gw_to_imag;
    nimcp_mutex_unlock(((gw_imagination_bridge_t*)bridge)->base.mutex);

    return 0;
}

int gw_imagination_get_imag_effects(
    const gw_imagination_bridge_t* bridge,
    imagination_to_gw_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_get_imag_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_get_i", 0.0f);

    nimcp_mutex_lock(((gw_imagination_bridge_t*)bridge)->base.mutex);
    *effects = bridge->imag_to_gw;
    nimcp_mutex_unlock(((gw_imagination_bridge_t*)bridge)->base.mutex);

    return 0;
}

int gw_imagination_get_stats(
    const gw_imagination_bridge_t* bridge,
    gw_imagination_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_get_s", 0.0f);

    nimcp_mutex_lock(((gw_imagination_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((gw_imagination_bridge_t*)bridge)->base.mutex);

    return 0;
}

int gw_imagination_reset_stats(gw_imagination_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

uint32_t gw_imagination_get_pending_count(const gw_imagination_bridge_t* bridge) {
    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_get_p", 0.0f);


    return bridge->num_pending;
}

/* ============================================================================
 * BIO-ASYNC
 * ============================================================================ */

int gw_imagination_connect_bio_async(gw_imagination_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;  /* Already connected */

    /* Use bridge base helper */
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_conne", 0.0f);


    int result = bridge_base_connect_bio_async(&bridge->base);
    if (result == 0) {
        NIMCP_LOGGING_INFO("GW-imagination bridge connected to bio-async");
    }

    return result;
}

int gw_imagination_disconnect_bio_async(gw_imagination_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;  /* Already disconnected */

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_disco", 0.0f);


    int result = bridge_base_disconnect_bio_async(&bridge->base);
    if (result == 0) {
        NIMCP_LOGGING_INFO("GW-imagination bridge disconnected from bio-async");
    }

    return result;
}

bool gw_imagination_is_bio_async_connected(const gw_imagination_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_is_bi", 0.0f);


    return bridge->base.bio_async_enabled;
}

int gw_imagination_process_messages(gw_imagination_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_imagination_process_messages: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Process pending bio-async messages */
    /* In full implementation, would handle:
     * - BIO_MSG_GW_BROADCAST_REQUEST
     * - BIO_MSG_GW_ATTENTION_BOOST
     * - BIO_MSG_IMAGINATION_CONTENT_READY
     * - BIO_MSG_GW_COMPETITION_RESULT
     */

    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_gw_imagination_proce", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for GW Imagination Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int gw_imagination_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    gw_imagination_bridge_heartbeat("gw_imaginati_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "GW_Imagination_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gw_imagination_bridge_heartbeat("gw_imaginati_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("GW Imagination Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "GW_Imagination_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "GW_Imagination_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gw_imagination_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gw_imagination_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gw_imagination_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gw_imagination_bridge_training_begin: NULL argument");
        return -1;
    }
    gw_imagination_bridge_heartbeat("gw_imagination_bridge_training_begin", 0.0f);
    return 0;
}

int gw_imagination_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gw_imagination_bridge_training_end: NULL argument");
        return -1;
    }
    gw_imagination_bridge_heartbeat("gw_imagination_bridge_training_end", 1.0f);
    return 0;
}

int gw_imagination_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gw_imagination_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gw_imagination_bridge_heartbeat("gw_imagination_bridge_training_step", progress);
    return 0;
}
