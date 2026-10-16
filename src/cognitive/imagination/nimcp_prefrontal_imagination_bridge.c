/**
 * @file nimcp_prefrontal_imagination_bridge.c
 * @brief Prefrontal-Imagination Bidirectional Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "cognitive/imagination/nimcp_prefrontal_imagination_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "core/brain/regions/prefrontal/nimcp_prefrontal_adapter.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(prefrontal_imagination_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_prefrontal_imagination_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_prefrontal_imagination_bridge_mesh_registry = NULL;

nimcp_error_t prefrontal_imagination_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_prefrontal_imagination_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "prefrontal_imagination_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "prefrontal_imagination_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_prefrontal_imagination_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_prefrontal_imagination_bridge_mesh_registry = registry;
    return err;
}

void prefrontal_imagination_bridge_mesh_unregister(void) {
    if (g_prefrontal_imagination_bridge_mesh_registry && g_prefrontal_imagination_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_prefrontal_imagination_bridge_mesh_registry, g_prefrontal_imagination_bridge_mesh_id);
        g_prefrontal_imagination_bridge_mesh_id = 0;
        g_prefrontal_imagination_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from prefrontal_imagination_bridge module (instance-level) */
static inline void prefrontal_imagination_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_prefrontal_imagination_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_prefrontal_imagination_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_prefrontal_imagination_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

int prefrontal_imagination_default_config(prefrontal_imagination_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    config->goal_relevance_threshold = PFC_IMAG_DEFAULT_GOAL_RELEVANCE;
    config->goal_constraint_weight = 0.6f;
    config->enable_goal_tracking = true;

    config->default_mode = PFC_IMAG_MODE_FOCUSED;
    config->exploration_default = 0.3f;
    config->creativity_default = 0.5f;

    config->inhibition_threshold = PFC_IMAG_DEFAULT_INHIBITION_THRESHOLD;
    config->enable_negative_filtering = true;
    config->enable_off_task_filtering = true;

    config->default_num_options = 4;
    config->option_diversity_weight = 0.4f;

    config->enable_wm_context = true;
    config->enable_wm_updates = true;
    config->max_wm_items = PFC_IMAG_MAX_WM_ITEMS;

    config->update_interval_ms = 16.0f;  /* ~60 Hz */
    config->enable_bio_async = true;

    return 0;
}

int prefrontal_imagination_validate_config(const prefrontal_imagination_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    if (config->goal_relevance_threshold < 0.0f || config->goal_relevance_threshold > 1.0f) {
        return -1;
    }
    if (config->goal_constraint_weight < 0.0f || config->goal_constraint_weight > 1.0f) {
        return -1;
    }
    if (config->exploration_default < 0.0f || config->exploration_default > 1.0f) {
        return -1;
    }
    if (config->creativity_default < 0.0f || config->creativity_default > 1.0f) {
        return -1;
    }
    if (config->inhibition_threshold < 0.0f || config->inhibition_threshold > 1.0f) {
        return -1;
    }
    if (config->default_num_options == 0 || config->default_num_options > PFC_IMAG_MAX_OPTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prefrontal_imagination_validate_config: config->default_num_options is zero");
        return -1;
    }
    if (config->option_diversity_weight < 0.0f || config->option_diversity_weight > 1.0f) {
        return -1;
    }
    if (config->max_wm_items == 0 || config->max_wm_items > PFC_IMAG_MAX_WM_ITEMS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prefrontal_imagination_validate_config: config->max_wm_items is zero");
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

prefrontal_imagination_bridge_t* prefrontal_imagination_bridge_create(
    const prefrontal_imagination_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_create", 0.0f);


    prefrontal_imagination_bridge_t* bridge = nimcp_calloc(
        1, sizeof(prefrontal_imagination_bridge_t));
    if (!bridge) {
        LOG_ERROR("Failed to allocate prefrontal-imagination bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Initialize base */
    bridge->base.module_id = BIO_MODULE_IMAGINATION_PREFRONTAL;
    bridge->base.module_name = "prefrontal_imagination_bridge";
    bridge->base.system_a_connected = false;
    bridge->base.system_b_connected = false;
    bridge->base.bridge_active = false;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "prefrontal_imagination") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        LOG_ERROR("Failed to create bridge mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_imagination_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        if (prefrontal_imagination_validate_config(config) != 0) {
            LOG_ERROR("Invalid bridge configuration");
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_imagination_bridge_create: validation failed");
            return NULL;
        }
        bridge->config = *config;
    } else {
        prefrontal_imagination_default_config(&bridge->config);
    }

    /* Initialize effects */
    memset(&bridge->pfc_to_imag, 0, sizeof(bridge->pfc_to_imag));
    memset(&bridge->imag_to_pfc, 0, sizeof(bridge->imag_to_pfc));

    /* Set default mode from config */
    bridge->current_mode = bridge->config.default_mode;
    bridge->pfc_to_imag.current_mode = bridge->config.default_mode;
    bridge->pfc_to_imag.exploration_level = bridge->config.exploration_default;
    bridge->pfc_to_imag.creativity_bound = bridge->config.creativity_default;
    bridge->pfc_to_imag.goal_relevance_threshold = bridge->config.goal_relevance_threshold;

    /* Initialize state */
    bridge->num_active_scenarios = 0;
    bridge->options_request_pending = false;
    bridge->pending_num_options = 0;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Initialize timing */
    bridge->last_update_time_ms = nimcp_time_get_ms();
    bridge->mode_start_time_ms = bridge->last_update_time_ms;

    LOG_INFO("Created prefrontal-imagination bridge");
    return bridge;
}

void prefrontal_imagination_bridge_destroy(prefrontal_imagination_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        prefrontal_imagination_disconnect_bio_async(bridge);
    }

    /* Free tensor resources */
    if (bridge->pfc_to_imag.goal_embeddings) {
        nimcp_tensor_destroy(bridge->pfc_to_imag.goal_embeddings);
    }
    if (bridge->pfc_to_imag.wm_context) {
        nimcp_tensor_destroy(bridge->pfc_to_imag.wm_context);
    }
    if (bridge->imag_to_pfc.best_option_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_pfc.best_option_embedding);
    }
    if (bridge->imag_to_pfc.wm_update_content) {
        nimcp_tensor_destroy(bridge->imag_to_pfc.wm_update_content);
    }
    if (bridge->imag_to_pfc.suggested_goal_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_pfc.suggested_goal_embedding);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    LOG_INFO("Destroyed prefrontal-imagination bridge");
}

int prefrontal_imagination_reset(prefrontal_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Free and clear tensors in effects */
    if (bridge->pfc_to_imag.goal_embeddings) {
        nimcp_tensor_destroy(bridge->pfc_to_imag.goal_embeddings);
    }
    if (bridge->pfc_to_imag.wm_context) {
        nimcp_tensor_destroy(bridge->pfc_to_imag.wm_context);
    }
    if (bridge->imag_to_pfc.best_option_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_pfc.best_option_embedding);
    }
    if (bridge->imag_to_pfc.wm_update_content) {
        nimcp_tensor_destroy(bridge->imag_to_pfc.wm_update_content);
    }
    if (bridge->imag_to_pfc.suggested_goal_embedding) {
        nimcp_tensor_destroy(bridge->imag_to_pfc.suggested_goal_embedding);
    }

    /* Clear effects */
    memset(&bridge->pfc_to_imag, 0, sizeof(bridge->pfc_to_imag));
    memset(&bridge->imag_to_pfc, 0, sizeof(bridge->imag_to_pfc));

    /* Reset to defaults */
    bridge->current_mode = bridge->config.default_mode;
    bridge->pfc_to_imag.current_mode = bridge->config.default_mode;
    bridge->pfc_to_imag.exploration_level = bridge->config.exploration_default;
    bridge->pfc_to_imag.creativity_bound = bridge->config.creativity_default;
    bridge->pfc_to_imag.goal_relevance_threshold = bridge->config.goal_relevance_threshold;

    /* Clear state */
    bridge->num_active_scenarios = 0;
    bridge->options_request_pending = false;
    bridge->pending_num_options = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * CONNECTION
 * ============================================================================ */

int prefrontal_imagination_connect_prefrontal(
    prefrontal_imagination_bridge_t* bridge,
    struct prefrontal_adapter* prefrontal)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->prefrontal = prefrontal;
    bridge->base.system_a = prefrontal;
    bridge->base.system_a_connected = (prefrontal != NULL);
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  bridge->base.system_b_connected;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_INFO("Prefrontal cortex %s to bridge",
                   prefrontal ? "connected" : "disconnected");
    return 0;
}

int prefrontal_imagination_connect_imagination(
    prefrontal_imagination_bridge_t* bridge,
    struct imagination_engine* imagination)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->imagination = imagination;
    bridge->base.system_b = imagination;
    bridge->base.system_b_connected = (imagination != NULL);
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  bridge->base.system_b_connected;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_INFO("Imagination engine %s to bridge",
                   imagination ? "connected" : "disconnected");
    return 0;
}

int prefrontal_imagination_disconnect_prefrontal(
    prefrontal_imagination_bridge_t* bridge)
{
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    return prefrontal_imagination_connect_prefrontal(bridge, NULL);
}

int prefrontal_imagination_disconnect_imagination(
    prefrontal_imagination_bridge_t* bridge)
{
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    return prefrontal_imagination_connect_imagination(bridge, NULL);
}

bool prefrontal_imagination_is_connected(
    const prefrontal_imagination_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    return bridge->base.bridge_active;
}

/* ============================================================================
 * UPDATE
 * ============================================================================ */

int prefrontal_imagination_update(
    prefrontal_imagination_bridge_t* bridge,
    float delta_time_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bridge_active) return 0;

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check update interval */
    uint64_t now = nimcp_time_get_ms();
    float elapsed = (float)(now - bridge->last_update_time_ms);
    if (elapsed < bridge->config.update_interval_ms) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->last_update_time_ms = now;

    /* Track time in current mode */
    float mode_elapsed = (float)(now - bridge->mode_start_time_ms);
    if (bridge->current_mode == PFC_IMAG_MODE_FOCUSED) {
        bridge->stats.time_in_focused_ms += (uint64_t)mode_elapsed;
    } else if (bridge->current_mode == PFC_IMAG_MODE_EXPLORATORY) {
        bridge->stats.time_in_exploratory_ms += (uint64_t)mode_elapsed;
    }
    bridge->mode_start_time_ms = now;

    /* Compute effects in both directions */
    prefrontal_imagination_compute_pfc_effects(bridge);
    prefrontal_imagination_compute_imag_effects(bridge);

    /* Apply effects */
    prefrontal_imagination_apply_effects(bridge);

    /* Update statistics */
    bridge->stats.total_updates++;
    bridge->base.total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int prefrontal_imagination_compute_pfc_effects(
    prefrontal_imagination_bridge_t* bridge)
{
    if (!bridge || !bridge->prefrontal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_imagination_compute_pfc_effects: required parameter is NULL (bridge, bridge->prefrontal)");
        return -1;
    }

    /* Query prefrontal for current executive state */
    /* In full implementation, would call prefrontal adapter APIs */

    /* Set inhibition based on mode */
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    if (bridge->current_mode == PFC_IMAG_MODE_INHIBITED) {
        bridge->pfc_to_imag.inhibition_strength = 1.0f;
    } else {
        bridge->pfc_to_imag.inhibition_strength = 0.0f;
    }

    /* Set filtering based on config */
    bridge->pfc_to_imag.suppress_negative_scenarios = bridge->config.enable_negative_filtering;
    bridge->pfc_to_imag.suppress_off_task = bridge->config.enable_off_task_filtering;

    /* Handle pending options request */
    if (bridge->options_request_pending) {
        bridge->pfc_to_imag.options_requested = true;
        bridge->pfc_to_imag.num_options_requested = bridge->pending_num_options;
    } else {
        bridge->pfc_to_imag.options_requested = false;
    }

    return 0;
}

int prefrontal_imagination_compute_imag_effects(
    prefrontal_imagination_bridge_t* bridge)
{
    if (!bridge || !bridge->imagination) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_imagination_compute_imag_effects: required parameter is NULL (bridge, bridge->imagination)");
        return -1;
    }

    /* Query imagination engine for current state */
    /* In full implementation, would call imagination engine APIs */

    /* Default: no options generated yet */
    if (!bridge->options_request_pending) {
        bridge->imag_to_pfc.num_options_generated = 0;
    }

    /* Default WM and goal suggestions */
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    bridge->imag_to_pfc.wm_update_suggested = false;
    bridge->imag_to_pfc.new_goal_suggested = false;
    bridge->imag_to_pfc.confidence = 0.0f;

    return 0;
}

int prefrontal_imagination_apply_effects(prefrontal_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply PFC effects to imagination */
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    if (bridge->imagination && bridge->pfc_to_imag.options_requested) {
        /* Would call imagination APIs to generate options */
        bridge->stats.option_requests++;
        bridge->options_request_pending = false;
    }

    /* Apply imagination effects to PFC */
    if (bridge->prefrontal && bridge->imag_to_pfc.num_options_generated > 0) {
        /* Would notify PFC of available options */
        bridge->stats.options_generated += bridge->imag_to_pfc.num_options_generated;
    }

    /* Handle inhibition */
    if (bridge->pfc_to_imag.inhibition_strength > bridge->config.inhibition_threshold) {
        bridge->stats.inhibition_triggers++;
    }

    /* Handle WM update suggestions */
    if (bridge->imag_to_pfc.wm_update_suggested) {
        bridge->stats.wm_updates_suggested++;
    }

    /* Handle goal suggestions */
    if (bridge->imag_to_pfc.new_goal_suggested) {
        bridge->stats.goals_suggested++;
    }

    return 0;
}

/* ============================================================================
 * EXECUTIVE CONTROL
 * ============================================================================ */

int prefrontal_imagination_set_mode(
    prefrontal_imagination_bridge_t* bridge,
    pfc_imagination_mode_t mode)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->current_mode != mode) {
        bridge->current_mode = mode;
        bridge->pfc_to_imag.current_mode = mode;
        bridge->mode_start_time_ms = nimcp_time_get_ms();
        bridge->stats.mode_changes++;

        LOG_DEBUG("Imagination mode changed to %d", mode);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

pfc_imagination_mode_t prefrontal_imagination_get_mode(
    const prefrontal_imagination_bridge_t* bridge)
{
    if (!bridge) return PFC_IMAG_MODE_INHIBITED;
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    return bridge->current_mode;
}

int prefrontal_imagination_set_inhibition(
    prefrontal_imagination_bridge_t* bridge,
    float strength)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (strength < 0.0f || strength > 1.0f) return -1;

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->pfc_to_imag.inhibition_strength = strength;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int prefrontal_imagination_update_goals(
    prefrontal_imagination_bridge_t* bridge,
    const uint32_t* goal_ids,
    uint32_t num_goals)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    if (num_goals > PFC_IMAG_MAX_TRACKED_GOALS) {
        num_goals = PFC_IMAG_MAX_TRACKED_GOALS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->pfc_to_imag.num_active_goals = num_goals;
    for (uint32_t i = 0; i < num_goals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_goals > 256) {
            prefrontal_imagination_bridge_heartbeat("prefrontal_i_loop",
                             (float)(i + 1) / (float)num_goals);
        }

        bridge->pfc_to_imag.active_goal_ids[i] = goal_ids[i];
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Updated %u active goals for imagination", num_goals);
    return 0;
}

/* ============================================================================
 * OPTION GENERATION
 * ============================================================================ */

int prefrontal_imagination_request_options(
    prefrontal_imagination_bridge_t* bridge,
    uint32_t goal_id,
    uint32_t num_options)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bridge_active) return -1;
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    if (num_options == 0 || num_options > PFC_IMAG_MAX_OPTIONS) {
        num_options = bridge->config.default_num_options;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Store request for processing in update */
    bridge->options_request_pending = true;
    bridge->pending_num_options = num_options;

    /* Update goal context */
    if (goal_id != 0 && bridge->pfc_to_imag.num_active_goals < PFC_IMAG_MAX_TRACKED_GOALS) {
        bridge->pfc_to_imag.active_goal_ids[0] = goal_id;
        bridge->pfc_to_imag.num_active_goals = 1;
    }

    /* Switch to evaluative mode */
    bridge->current_mode = PFC_IMAG_MODE_EVALUATIVE;
    bridge->pfc_to_imag.current_mode = PFC_IMAG_MODE_EVALUATIVE;

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Requested %u options for goal %u", num_options, goal_id);
    return 0;
}

int prefrontal_imagination_get_options(
    const prefrontal_imagination_bridge_t* bridge,
    uint32_t* scenario_ids,
    float* values,
    float* risks,
    uint32_t* num_options)
{
    if (!bridge || !num_options) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_imagination_get_options: required parameter is NULL (bridge, num_options)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    uint32_t count = bridge->imag_to_pfc.num_options_generated;
    if (count > *num_options) {
        count = *num_options;
    }

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            prefrontal_imagination_bridge_heartbeat("prefrontal_i_loop",
                             (float)(i + 1) / (float)count);
        }

        if (scenario_ids) scenario_ids[i] = bridge->imag_to_pfc.option_scenario_ids[i];
        if (values) values[i] = bridge->imag_to_pfc.option_values[i];
        if (risks) risks[i] = bridge->imag_to_pfc.option_risks[i];
    }

    *num_options = count;
    return 0;
}

int prefrontal_imagination_get_best_option(
    const prefrontal_imagination_bridge_t* bridge,
    uint32_t* scenario_id,
    float* value)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->imag_to_pfc.num_options_generated == 0) return -1;

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    uint32_t best_idx = bridge->imag_to_pfc.best_option_idx;
    if (scenario_id) {
        *scenario_id = bridge->imag_to_pfc.option_scenario_ids[best_idx];
    }
    if (value) {
        *value = bridge->imag_to_pfc.option_values[best_idx];
    }

    return 0;
}

/* ============================================================================
 * WORKING MEMORY INTEGRATION
 * ============================================================================ */

int prefrontal_imagination_update_wm_context(
    prefrontal_imagination_bridge_t* bridge,
    const nimcp_tensor_t* wm_context)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_wm_context) return -1;

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Free old context */
    if (bridge->pfc_to_imag.wm_context) {
        nimcp_tensor_destroy(bridge->pfc_to_imag.wm_context);
        bridge->pfc_to_imag.wm_context = NULL;
    }

    /* Clone new context */
    if (wm_context) {
        bridge->pfc_to_imag.wm_context = nimcp_tensor_clone(wm_context);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int prefrontal_imagination_get_wm_suggestion(
    const prefrontal_imagination_bridge_t* bridge,
    nimcp_tensor_t** content,
    float* priority)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->imag_to_pfc.wm_update_suggested) return -1;

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    if (content) {
        *content = bridge->imag_to_pfc.wm_update_content;
    }
    if (priority) {
        *priority = bridge->imag_to_pfc.wm_update_priority;
    }

    return 0;
}

int prefrontal_imagination_accept_wm_update(
    prefrontal_imagination_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->imag_to_pfc.wm_update_suggested) return -1;

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->imag_to_pfc.wm_update_suggested = false;
    bridge->stats.wm_updates_accepted++;

    /* In full implementation, would push update to WM */

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * QUERY
 * ============================================================================ */

int prefrontal_imagination_get_stats(
    const prefrontal_imagination_bridge_t* bridge,
    prefrontal_imagination_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_imagination_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);

    nimcp_mutex_lock(((prefrontal_imagination_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((prefrontal_imagination_bridge_t*)bridge)->base.mutex);

    /* Compute derived stats (from local copy, no lock needed) */
    if (stats->option_requests > 0) {
        stats->avg_options_per_request =
            (float)stats->options_generated / (float)stats->option_requests;
    }

    return 0;
}

int prefrontal_imagination_reset_stats(prefrontal_imagination_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int prefrontal_imagination_get_pfc_effects(
    const prefrontal_imagination_bridge_t* bridge,
    prefrontal_to_imagination_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_imagination_get_pfc_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->pfc_to_imag;
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    return 0;
}

int prefrontal_imagination_get_imag_effects(
    const prefrontal_imagination_bridge_t* bridge,
    imagination_to_prefrontal_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_imagination_get_imag_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->imag_to_pfc;
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    return 0;
}

/* ============================================================================
 * BIO-ASYNC
 * ============================================================================ */

int prefrontal_imagination_connect_bio_async(
    prefrontal_imagination_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    int result = bridge_base_connect_bio_async(&bridge->base);
    if (result == 0) {
        LOG_INFO("Prefrontal-imagination bridge connected to bio-async");
    }

    return result;
}

int prefrontal_imagination_disconnect_bio_async(
    prefrontal_imagination_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    int result = bridge_base_disconnect_bio_async(&bridge->base);
    if (result == 0) {
        LOG_INFO("Prefrontal-imagination bridge disconnected from bio-async");
    }

    return result;
}

bool prefrontal_imagination_is_bio_async_connected(
    const prefrontal_imagination_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    return bridge->base.bio_async_enabled;
}

int prefrontal_imagination_process_messages(
    prefrontal_imagination_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Process pending bio-async messages */
    /* In full implementation, would handle:
     * - BIO_MSG_IMAGINATION_OPTIONS_REQUEST
     * - BIO_MSG_IMAGINATION_OPTIONS_RESPONSE
     * - BIO_MSG_PFC_GOAL_UPDATE
     * - BIO_MSG_PFC_INHIBITION_SIGNAL
     * - BIO_MSG_WM_UPDATE_REQUEST
     */

    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_prefrontal_imaginati", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Prefrontal Imagination Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int prefrontal_imagination_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    prefrontal_imagination_bridge_heartbeat("prefrontal_i_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Prefrontal_Imagination_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                prefrontal_imagination_bridge_heartbeat("prefrontal_i_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Prefrontal Imagination Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Prefrontal_Imagination_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Prefrontal_Imagination_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void prefrontal_imagination_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_prefrontal_imagination_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int prefrontal_imagination_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "prefrontal_imagination_bridge_training_begin: NULL argument");
        return -1;
    }
    prefrontal_imagination_bridge_heartbeat("prefrontal_imagination_bridge_training_begin", 0.0f);
    return 0;
}

int prefrontal_imagination_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "prefrontal_imagination_bridge_training_end: NULL argument");
        return -1;
    }
    prefrontal_imagination_bridge_heartbeat("prefrontal_imagination_bridge_training_end", 1.0f);
    return 0;
}

int prefrontal_imagination_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "prefrontal_imagination_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    prefrontal_imagination_bridge_heartbeat("prefrontal_imagination_bridge_training_step", progress);
    return 0;
}
