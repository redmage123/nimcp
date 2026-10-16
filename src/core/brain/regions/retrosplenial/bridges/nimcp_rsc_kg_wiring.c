//=============================================================================
// nimcp_rsc_kg_wiring.c - Retrosplenial Cortex Knowledge Graph Registration
//=============================================================================
/**
 * @file nimcp_rsc_kg_wiring.c
 * @brief Implementation of RSC Knowledge Graph registration
 *
 * WHAT: Registers RSC concepts in the brain's internal Knowledge Graph
 * WHY:  Enable semantic queries and reasoning about spatial processing
 * HOW:  Creates hierarchical node structure with reference frames, contexts,
 *       navigation components, and imagination modes
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#include "core/brain/regions/retrosplenial/bridges/nimcp_rsc_kg_wiring.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rsc_kg_wiring, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Constants
//=============================================================================

static const char* RSC_ROOT_DESC =
    "Retrosplenial Cortex - spatial-contextual integration hub";
static const char* FRAMES_SUBSYSTEM_DESC =
    "Reference frame transformation system";
static const char* CONTEXT_SUBSYSTEM_DESC =
    "Context encoding and binding system";
static const char* NAVIGATION_SUBSYSTEM_DESC =
    "Navigation support and head direction integration";
static const char* IMAGINATION_SUBSYSTEM_DESC =
    "Mental simulation and prospective/retrospective processing";

//=============================================================================
// Configuration API
//=============================================================================

int rsc_kg_default_config(rsc_kg_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }
    config->register_frames = true;
    config->register_contexts = true;
    config->register_navigation = true;
    config->register_imagination = true;
    config->register_transform_edges = true;
    config->register_context_edges = true;
    config->register_cross_edges = true;
    config->include_state_metadata = true;
    return 0;
}

//=============================================================================
// Internal Helpers
//=============================================================================

/** Initialize state with invalid node IDs */
static void init_state(rsc_kg_state_t* state)
{
    if (!state) {
        return;
    }
    memset(state, 0, sizeof(rsc_kg_state_t));
    state->root_id = BRAIN_KG_INVALID_NODE;
    state->frames_subsystem_id = BRAIN_KG_INVALID_NODE;
    state->context_subsystem_id = BRAIN_KG_INVALID_NODE;
    state->navigation_subsystem_id = BRAIN_KG_INVALID_NODE;
    state->imagination_subsystem_id = BRAIN_KG_INVALID_NODE;
    state->egocentric_id = BRAIN_KG_INVALID_NODE;
    state->allocentric_id = BRAIN_KG_INVALID_NODE;
    state->object_centered_id = BRAIN_KG_INVALID_NODE;
    state->route_centered_id = BRAIN_KG_INVALID_NODE;
    state->spatial_context_id = BRAIN_KG_INVALID_NODE;
    state->temporal_context_id = BRAIN_KG_INVALID_NODE;
    state->environmental_context_id = BRAIN_KG_INVALID_NODE;
    state->social_context_id = BRAIN_KG_INVALID_NODE;
    state->emotional_context_id = BRAIN_KG_INVALID_NODE;
    state->task_context_id = BRAIN_KG_INVALID_NODE;
    state->head_direction_id = BRAIN_KG_INVALID_NODE;
    state->landmarks_id = BRAIN_KG_INVALID_NODE;
    state->scene_recognition_id = BRAIN_KG_INVALID_NODE;
    state->prospective_id = BRAIN_KG_INVALID_NODE;
    state->retrospective_id = BRAIN_KG_INVALID_NODE;
    state->counterfactual_id = BRAIN_KG_INVALID_NODE;
    state->registered = false;
}

/** Helper: add child node with edge to parent */
static brain_kg_node_id_t add_child_node(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    const char* name,
    const char* desc,
    const char* edge_desc,
    rsc_kg_state_t* state)
{
    brain_kg_node_id_t id = brain_kg_add_node(kg, name, BRAIN_KG_NODE_CUSTOM, desc);
    if (id != BRAIN_KG_INVALID_NODE) {
        state->node_count++;
        brain_kg_add_edge(kg, parent_id, id,
            BRAIN_KG_EDGE_CONNECTS_TO, edge_desc, 1.0f);
        state->edge_count++;
    }
    return id;
}

/** Helper: remove node if valid */
static void remove_node_if_valid(brain_kg_t* kg, brain_kg_node_id_t id)
{
    if (id != BRAIN_KG_INVALID_NODE) {
        brain_kg_remove_node(kg, id);
    }
}

//=============================================================================
// Registration: Reference Frames
//=============================================================================

/** Create frames subsystem node */
static int create_frames_subsystem(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rsc_kg_state_t* state)
{
    state->frames_subsystem_id = brain_kg_add_node(
        kg, RSC_KG_FRAMES_NAME, BRAIN_KG_NODE_INTEGRATION, FRAMES_SUBSYSTEM_DESC);
    if (state->frames_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "create_frames_subsystem: validation failed");
        return -1;
    }
    state->node_count++;
    brain_kg_add_edge(kg, parent_id, state->frames_subsystem_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "RSC contains reference frames", 1.0f);
    state->edge_count++;
    return 0;
}

/** Create frame child nodes */
static void create_frame_nodes(brain_kg_t* kg, rsc_kg_state_t* state)
{
    brain_kg_node_id_t p = state->frames_subsystem_id;
    state->egocentric_id = add_child_node(kg, p, "egocentric_frame",
        "Body-centered spatial reference frame", "contains egocentric", state);
    state->allocentric_id = add_child_node(kg, p, "allocentric_frame",
        "World-centered spatial reference frame", "contains allocentric", state);
    state->object_centered_id = add_child_node(kg, p, "object_centered_frame",
        "Object-relative spatial reference frame", "contains object-centered", state);
    state->route_centered_id = add_child_node(kg, p, "route_centered_frame",
        "Route/path-relative spatial reference frame", "contains route-centered", state);
}

int rsc_kg_register_frames(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rsc_kg_state_t* state,
    uint64_t admin_token)
{
    (void)admin_token;
    if (!kg || !state || parent_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_register_frames: required parameter is NULL (kg, state)");
        return -1;
    }
    if (create_frames_subsystem(kg, parent_id, state) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_kg_register_frames: validation failed");
        return -1;
    }
    create_frame_nodes(kg, state);
    return 0;
}

//=============================================================================
// Registration: Context Types
//=============================================================================

/** Create context subsystem node */
static int create_context_subsystem(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rsc_kg_state_t* state)
{
    state->context_subsystem_id = brain_kg_add_node(
        kg, RSC_KG_CONTEXT_NAME, BRAIN_KG_NODE_INTEGRATION, CONTEXT_SUBSYSTEM_DESC);
    if (state->context_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "create_context_subsystem: validation failed");
        return -1;
    }
    state->node_count++;
    brain_kg_add_edge(kg, parent_id, state->context_subsystem_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "RSC contains context types", 1.0f);
    state->edge_count++;
    return 0;
}

/** Create context child nodes */
static void create_context_nodes(brain_kg_t* kg, rsc_kg_state_t* state)
{
    brain_kg_node_id_t p = state->context_subsystem_id;
    state->spatial_context_id = add_child_node(kg, p, "spatial_context",
        "Spatial/location context encoding", "contains spatial", state);
    state->temporal_context_id = add_child_node(kg, p, "temporal_context",
        "Temporal/time context encoding", "contains temporal", state);
    state->environmental_context_id = add_child_node(kg, p, "environmental_context",
        "Environmental feature context encoding", "contains environmental", state);
    state->social_context_id = add_child_node(kg, p, "social_context",
        "Social context encoding", "contains social", state);
    state->emotional_context_id = add_child_node(kg, p, "emotional_context",
        "Emotional context encoding", "contains emotional", state);
    state->task_context_id = add_child_node(kg, p, "task_context",
        "Task/goal context encoding", "contains task", state);
}

int rsc_kg_register_contexts(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rsc_kg_state_t* state,
    uint64_t admin_token)
{
    (void)admin_token;
    if (!kg || !state || parent_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_register_contexts: required parameter is NULL (kg, state)");
        return -1;
    }
    if (create_context_subsystem(kg, parent_id, state) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_kg_register_contexts: validation failed");
        return -1;
    }
    create_context_nodes(kg, state);
    return 0;
}

//=============================================================================
// Registration: Navigation Components
//=============================================================================

/** Create navigation subsystem node */
static int create_navigation_subsystem(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rsc_kg_state_t* state)
{
    state->navigation_subsystem_id = brain_kg_add_node(
        kg, RSC_KG_NAVIGATION_NAME, BRAIN_KG_NODE_INTEGRATION, NAVIGATION_SUBSYSTEM_DESC);
    if (state->navigation_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "create_navigation_subsystem: validation failed");
        return -1;
    }
    state->node_count++;
    brain_kg_add_edge(kg, parent_id, state->navigation_subsystem_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "RSC contains navigation", 1.0f);
    state->edge_count++;
    return 0;
}

/** Create navigation child nodes */
static void create_navigation_nodes(brain_kg_t* kg, rsc_kg_state_t* state)
{
    brain_kg_node_id_t p = state->navigation_subsystem_id;
    state->head_direction_id = add_child_node(kg, p, "head_direction",
        "Head direction signal integration from anterior thalamus",
        "contains head direction", state);
    state->landmarks_id = add_child_node(kg, p, "landmarks",
        "Landmark recognition and spatial anchoring", "contains landmarks", state);
    state->scene_recognition_id = add_child_node(kg, p, "scene_recognition",
        "Scene familiarity and layout processing", "contains scene recognition", state);
}

int rsc_kg_register_navigation(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rsc_kg_state_t* state,
    uint64_t admin_token)
{
    (void)admin_token;
    if (!kg || !state || parent_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_register_navigation: required parameter is NULL (kg, state)");
        return -1;
    }
    if (create_navigation_subsystem(kg, parent_id, state) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_kg_register_navigation: validation failed");
        return -1;
    }
    create_navigation_nodes(kg, state);
    return 0;
}

//=============================================================================
// Registration: Imagination Modes
//=============================================================================

/** Create imagination subsystem node */
static int create_imagination_subsystem(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rsc_kg_state_t* state)
{
    state->imagination_subsystem_id = brain_kg_add_node(
        kg, RSC_KG_IMAGINATION_NAME, BRAIN_KG_NODE_INTEGRATION, IMAGINATION_SUBSYSTEM_DESC);
    if (state->imagination_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "create_imagination_subsystem: validation failed");
        return -1;
    }
    state->node_count++;
    brain_kg_add_edge(kg, parent_id, state->imagination_subsystem_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "RSC contains imagination", 1.0f);
    state->edge_count++;
    return 0;
}

/** Create imagination child nodes */
static void create_imagination_nodes(brain_kg_t* kg, rsc_kg_state_t* state)
{
    brain_kg_node_id_t p = state->imagination_subsystem_id;
    state->prospective_id = add_child_node(kg, p, "prospective_imagination",
        "Future scenario mental simulation", "contains prospective", state);
    state->retrospective_id = add_child_node(kg, p, "retrospective_imagination",
        "Past event reconstruction and replay", "contains retrospective", state);
    state->counterfactual_id = add_child_node(kg, p, "counterfactual_imagination",
        "What-if scenario mental simulation", "contains counterfactual", state);
}

int rsc_kg_register_imagination(
    brain_kg_t* kg,
    brain_kg_node_id_t parent_id,
    rsc_kg_state_t* state,
    uint64_t admin_token)
{
    (void)admin_token;
    if (!kg || !state || parent_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_register_imagination: required parameter is NULL (kg, state)");
        return -1;
    }
    if (create_imagination_subsystem(kg, parent_id, state) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_kg_register_imagination: validation failed");
        return -1;
    }
    create_imagination_nodes(kg, state);
    return 0;
}

//=============================================================================
// Registration: Edges
//=============================================================================

int rsc_kg_register_transform_edges(
    brain_kg_t* kg,
    rsc_kg_state_t* state,
    uint64_t admin_token)
{
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_register_transform_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Egocentric <-> Allocentric bidirectional */
    if (state->egocentric_id != BRAIN_KG_INVALID_NODE &&
        state->allocentric_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, state->egocentric_id, state->allocentric_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "Ego-to-allo transformation", 0.9f);
        brain_kg_add_edge(kg, state->allocentric_id, state->egocentric_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "Allo-to-ego transformation", 0.9f);
        state->edge_count += 2;
    }

    /* Allocentric -> Object-centered */
    if (state->allocentric_id != BRAIN_KG_INVALID_NODE &&
        state->object_centered_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, state->allocentric_id, state->object_centered_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "Allo-to-object transformation", 0.7f);
        state->edge_count++;
    }

    /* Route-centered -> Allocentric */
    if (state->route_centered_id != BRAIN_KG_INVALID_NODE &&
        state->allocentric_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, state->route_centered_id, state->allocentric_id,
            BRAIN_KG_EDGE_CONNECTS_TO, "Route-to-allo transformation", 0.8f);
        state->edge_count++;
    }
    return 0;
}

int rsc_kg_register_context_edges(
    brain_kg_t* kg,
    rsc_kg_state_t* state,
    uint64_t admin_token)
{
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_register_context_edges: required parameter is NULL (kg, state)");
        return -1;
    }

    /* Spatial context -> Allocentric frame */
    if (state->spatial_context_id != BRAIN_KG_INVALID_NODE &&
        state->allocentric_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, state->spatial_context_id, state->allocentric_id,
            BRAIN_KG_EDGE_INTEGRATES_WITH, "Spatial context uses allocentric", 0.9f);
        state->edge_count++;
    }

    /* Environmental context -> Scene recognition */
    if (state->environmental_context_id != BRAIN_KG_INVALID_NODE &&
        state->scene_recognition_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, state->environmental_context_id, state->scene_recognition_id,
            BRAIN_KG_EDGE_INTEGRATES_WITH, "Environmental uses scene", 0.85f);
        state->edge_count++;
    }

    /* Temporal context -> Imagination */
    if (state->temporal_context_id != BRAIN_KG_INVALID_NODE &&
        state->imagination_subsystem_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, state->temporal_context_id, state->imagination_subsystem_id,
            BRAIN_KG_EDGE_MODULATES, "Temporal modulates imagination", 0.8f);
        state->edge_count++;
    }
    return 0;
}

/** Helper: add navigation-to-frame edges */
static void add_nav_frame_edges(brain_kg_t* kg, rsc_kg_state_t* state)
{
    if (state->head_direction_id != BRAIN_KG_INVALID_NODE &&
        state->frames_subsystem_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, state->head_direction_id, state->frames_subsystem_id,
            BRAIN_KG_EDGE_MODULATES, "HD modulates frame transforms", 0.95f);
        state->edge_count++;
    }
    if (state->landmarks_id != BRAIN_KG_INVALID_NODE &&
        state->allocentric_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, state->landmarks_id, state->allocentric_id,
            BRAIN_KG_EDGE_MODULATES, "Landmarks anchor allocentric", 0.9f);
        state->edge_count++;
    }
}

/** Helper: add scene-context and imagination edges */
static void add_imagination_edges(brain_kg_t* kg, rsc_kg_state_t* state)
{
    if (state->scene_recognition_id != BRAIN_KG_INVALID_NODE &&
        state->context_subsystem_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, state->scene_recognition_id, state->context_subsystem_id,
            BRAIN_KG_EDGE_SENDS_TO, "Scene informs context", 0.85f);
        state->edge_count++;
    }
    if (state->imagination_subsystem_id != BRAIN_KG_INVALID_NODE &&
        state->navigation_subsystem_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, state->imagination_subsystem_id, state->navigation_subsystem_id,
            BRAIN_KG_EDGE_RECEIVES_FROM, "Imagination uses navigation", 0.8f);
        state->edge_count++;
    }
    if (state->prospective_id != BRAIN_KG_INVALID_NODE &&
        state->allocentric_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, state->prospective_id, state->allocentric_id,
            BRAIN_KG_EDGE_RECEIVES_FROM, "Prospective uses allocentric", 0.85f);
        state->edge_count++;
    }
    if (state->retrospective_id != BRAIN_KG_INVALID_NODE &&
        state->temporal_context_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, state->retrospective_id, state->temporal_context_id,
            BRAIN_KG_EDGE_RECEIVES_FROM, "Retrospective uses temporal", 0.9f);
        state->edge_count++;
    }
}

int rsc_kg_register_cross_edges(
    brain_kg_t* kg,
    rsc_kg_state_t* state,
    uint64_t admin_token)
{
    (void)admin_token;
    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_register_cross_edges: required parameter is NULL (kg, state)");
        return -1;
    }
    add_nav_frame_edges(kg, state);
    add_imagination_edges(kg, state);
    return 0;
}

//=============================================================================
// Registration: Register All
//=============================================================================

/** Helper: register all subsystem nodes */
static int register_subsystems(
    brain_kg_t* kg,
    const rsc_kg_config_t* config,
    rsc_kg_state_t* state,
    uint64_t admin_token)
{
    if (config->register_frames &&
        rsc_kg_register_frames(kg, state->root_id, state, admin_token) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "register_subsystems: operation failed");
        return -1;
    }
    if (config->register_contexts &&
        rsc_kg_register_contexts(kg, state->root_id, state, admin_token) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "register_subsystems: operation failed");
        return -1;
    }
    if (config->register_navigation &&
        rsc_kg_register_navigation(kg, state->root_id, state, admin_token) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "register_subsystems: operation failed");
        return -1;
    }
    if (config->register_imagination &&
        rsc_kg_register_imagination(kg, state->root_id, state, admin_token) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "register_subsystems: operation failed");
        return -1;
    }
    return 0;
}

/** Helper: register all edge types */
static void register_edge_types(
    brain_kg_t* kg,
    const rsc_kg_config_t* config,
    rsc_kg_state_t* state,
    uint64_t admin_token)
{
    if (config->register_transform_edges) {
        rsc_kg_register_transform_edges(kg, state, admin_token);
    }
    if (config->register_context_edges) {
        rsc_kg_register_context_edges(kg, state, admin_token);
    }
    if (config->register_cross_edges) {
        rsc_kg_register_cross_edges(kg, state, admin_token);
    }
}

int rsc_kg_register_all(
    brain_kg_t* kg,
    const rsc_kg_config_t* config,
    rsc_kg_state_t* state,
    uint64_t admin_token)
{
    rsc_kg_config_t local_config;
    rsc_kg_state_t local_state;

    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");


        return -1;
    }
    if (!state) {
        state = &local_state;
    }
    init_state(state);
    if (!config) {
        rsc_kg_default_config(&local_config);
        config = &local_config;
    }

    /* Create root node */
    state->root_id = brain_kg_add_node(
        kg, RSC_KG_ROOT_NAME, BRAIN_KG_NODE_CORTICAL, RSC_ROOT_DESC);
    if (state->root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_kg_register_all: validation failed");
        return -1;
    }
    state->node_count++;

    /* Register subsystems and edges */
    if (register_subsystems(kg, config, state, admin_token) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_kg_register_all: validation failed");
        return -1;
    }
    register_edge_types(kg, config, state, admin_token);

    state->registered = true;
    return 0;
}

//=============================================================================
// State Synchronization
//=============================================================================

int rsc_kg_update_state(
    brain_kg_t* kg,
    const rsc_kg_state_t* state,
    float transform_accuracy,
    float context_strength,
    float scene_familiarity,
    float head_direction,
    uint64_t admin_token)
{
    (void)admin_token;
    char value_str[NIMCP_ID_BUFFER_SIZE];

    if (!kg || !state || !state->registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_update_state: required parameter is NULL (kg, state, state->registered)");
        return -1;
    }

    if (state->root_id != BRAIN_KG_INVALID_NODE) {
        snprintf(value_str, sizeof(value_str), "%.3f", transform_accuracy);
        brain_kg_add_metadata(kg, state->root_id, "transform_accuracy", value_str);
        snprintf(value_str, sizeof(value_str), "%.3f", context_strength);
        brain_kg_add_metadata(kg, state->root_id, "context_strength", value_str);
    }
    if (state->scene_recognition_id != BRAIN_KG_INVALID_NODE) {
        snprintf(value_str, sizeof(value_str), "%.3f", scene_familiarity);
        brain_kg_add_metadata(kg, state->scene_recognition_id, "familiarity", value_str);
    }
    if (state->head_direction_id != BRAIN_KG_INVALID_NODE) {
        snprintf(value_str, sizeof(value_str), "%.4f", head_direction);
        brain_kg_add_metadata(kg, state->head_direction_id, "direction_rad", value_str);
    }
    return 0;
}

int rsc_kg_update_active_frame(
    brain_kg_t* kg,
    const rsc_kg_state_t* state,
    uint32_t active_frame,
    uint64_t admin_token)
{
    (void)admin_token;
    brain_kg_node_id_t frame_ids[4];

    if (!kg || !state || !state->registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_update_active_frame: required parameter is NULL (kg, state, state->registered)");
        return -1;
    }

    frame_ids[0] = state->egocentric_id;
    frame_ids[1] = state->allocentric_id;
    frame_ids[2] = state->object_centered_id;
    frame_ids[3] = state->route_centered_id;

    for (uint32_t i = 0; i < 4; i++) {
        if (frame_ids[i] != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_metadata(kg, frame_ids[i], "active",
                (i == active_frame) ? "true" : "false");
        }
    }
    return 0;
}

int rsc_kg_update_imagination_state(
    brain_kg_t* kg,
    const rsc_kg_state_t* state,
    bool active,
    uint32_t mode,
    float vividness,
    uint64_t admin_token)
{
    (void)admin_token;
    char value_str[NIMCP_ID_BUFFER_SIZE];

    if (!kg || !state || !state->registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_update_imagination_state: required parameter is NULL (kg, state, state->registered)");
        return -1;
    }
    if (state->imagination_subsystem_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_kg_update_imagination_state: validation failed");
        return -1;
    }

    brain_kg_add_metadata(kg, state->imagination_subsystem_id, "active",
        active ? "true" : "false");
    snprintf(value_str, sizeof(value_str), "%u", mode);
    brain_kg_add_metadata(kg, state->imagination_subsystem_id, "mode", value_str);
    snprintf(value_str, sizeof(value_str), "%.3f", vividness);
    brain_kg_add_metadata(kg, state->imagination_subsystem_id, "vividness", value_str);
    return 0;
}

//=============================================================================
// Query API
//=============================================================================

brain_kg_node_id_t rsc_kg_get_root(brain_kg_t* kg)
{
    if (!kg) {
        return BRAIN_KG_INVALID_NODE;
    }
    return brain_kg_find_node(kg, RSC_KG_ROOT_NAME);
}

brain_kg_node_id_t rsc_kg_find_subsystem(brain_kg_t* kg, const char* name)
{
    if (!kg || !name) {
        return BRAIN_KG_INVALID_NODE;
    }
    return brain_kg_find_node(kg, name);
}

brain_kg_node_list_t* rsc_kg_get_reference_frames(brain_kg_t* kg)
{
    brain_kg_node_id_t id;
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;
    }
    id = brain_kg_find_node(kg, RSC_KG_FRAMES_NAME);
    if (id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_get_reference_frames: validation failed");
        return NULL;
    }
    return brain_kg_get_neighbors(kg, id);
}

brain_kg_node_list_t* rsc_kg_get_context_types(brain_kg_t* kg)
{
    brain_kg_node_id_t id;
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;
    }
    id = brain_kg_find_node(kg, RSC_KG_CONTEXT_NAME);
    if (id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_get_context_types: validation failed");
        return NULL;
    }
    return brain_kg_get_neighbors(kg, id);
}

brain_kg_node_list_t* rsc_kg_get_navigation_components(brain_kg_t* kg)
{
    brain_kg_node_id_t id;
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;
    }
    id = brain_kg_find_node(kg, RSC_KG_NAVIGATION_NAME);
    if (id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_get_navigation_components: validation failed");
        return NULL;
    }
    return brain_kg_get_neighbors(kg, id);
}

brain_kg_node_list_t* rsc_kg_get_imagination_modes(brain_kg_t* kg)
{
    brain_kg_node_id_t id;
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;
    }
    id = brain_kg_find_node(kg, RSC_KG_IMAGINATION_NAME);
    if (id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_get_imagination_modes: validation failed");
        return NULL;
    }
    return brain_kg_get_neighbors(kg, id);
}

brain_kg_node_id_t rsc_kg_find_frame_by_type(brain_kg_t* kg, uint32_t frame_type)
{
    static const char* names[] = {
        "egocentric_frame", "allocentric_frame",
        "object_centered_frame", "route_centered_frame"
    };
    if (!kg || frame_type >= 4) {
        return BRAIN_KG_INVALID_NODE;
    }
    return brain_kg_find_node(kg, names[frame_type]);
}

brain_kg_node_id_t rsc_kg_find_context_by_type(brain_kg_t* kg, uint32_t context_type)
{
    static const char* names[] = {
        "spatial_context", "temporal_context", "environmental_context",
        "social_context", "emotional_context", "task_context"
    };
    if (!kg || context_type >= 6) {
        return BRAIN_KG_INVALID_NODE;
    }
    return brain_kg_find_node(kg, names[context_type]);
}

//=============================================================================
// Cleanup: Unregister helpers
//=============================================================================

/** Remove imagination nodes */
static void unregister_imagination_nodes(brain_kg_t* kg, rsc_kg_state_t* state)
{
    remove_node_if_valid(kg, state->counterfactual_id);
    remove_node_if_valid(kg, state->retrospective_id);
    remove_node_if_valid(kg, state->prospective_id);
}

/** Remove navigation nodes */
static void unregister_navigation_nodes(brain_kg_t* kg, rsc_kg_state_t* state)
{
    remove_node_if_valid(kg, state->scene_recognition_id);
    remove_node_if_valid(kg, state->landmarks_id);
    remove_node_if_valid(kg, state->head_direction_id);
}

/** Remove context nodes */
static void unregister_context_nodes(brain_kg_t* kg, rsc_kg_state_t* state)
{
    remove_node_if_valid(kg, state->task_context_id);
    remove_node_if_valid(kg, state->emotional_context_id);
    remove_node_if_valid(kg, state->social_context_id);
    remove_node_if_valid(kg, state->environmental_context_id);
    remove_node_if_valid(kg, state->temporal_context_id);
    remove_node_if_valid(kg, state->spatial_context_id);
}

/** Remove frame nodes */
static void unregister_frame_nodes(brain_kg_t* kg, rsc_kg_state_t* state)
{
    remove_node_if_valid(kg, state->route_centered_id);
    remove_node_if_valid(kg, state->object_centered_id);
    remove_node_if_valid(kg, state->allocentric_id);
    remove_node_if_valid(kg, state->egocentric_id);
}

/** Remove subsystem nodes */
static void unregister_subsystem_nodes(brain_kg_t* kg, rsc_kg_state_t* state)
{
    remove_node_if_valid(kg, state->imagination_subsystem_id);
    remove_node_if_valid(kg, state->navigation_subsystem_id);
    remove_node_if_valid(kg, state->context_subsystem_id);
    remove_node_if_valid(kg, state->frames_subsystem_id);
}

int rsc_kg_unregister_all(
    brain_kg_t* kg,
    rsc_kg_state_t* state,
    uint64_t admin_token)
{
    (void)admin_token;

    if (!kg || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_kg_unregister_all: required parameter is NULL (kg, state)");
        return -1;
    }
    if (!state->registered) {
        return 0;
    }

    /* Remove in reverse order */
    unregister_imagination_nodes(kg, state);
    unregister_navigation_nodes(kg, state);
    unregister_context_nodes(kg, state);
    unregister_frame_nodes(kg, state);
    unregister_subsystem_nodes(kg, state);
    remove_node_if_valid(kg, state->root_id);

    init_state(state);
    return 0;
}

//=============================================================================
// String Conversion
//=============================================================================

const char* rsc_kg_node_type_to_string(rsc_kg_node_type_t type)
{
    switch (type) {
        case RSC_KG_NODE_REFERENCE_FRAME:    return "reference_frame";
        case RSC_KG_NODE_CONTEXT_TYPE:       return "context_type";
        case RSC_KG_NODE_NAVIGATION_COMPONENT: return "navigation_component";
        case RSC_KG_NODE_IMAGINATION_MODE:   return "imagination_mode";
        case RSC_KG_NODE_LANDMARK:           return "landmark";
        case RSC_KG_NODE_SCENE:              return "scene";
        case RSC_KG_NODE_TRANSFORM_PROCESS:  return "transform_process";
        case RSC_KG_NODE_ENCODING_PROCESS:   return "encoding_process";
        case RSC_KG_NODE_MEMORY_BINDING:     return "memory_binding";
        default:                             return "unknown";
    }
}

const char* rsc_kg_edge_type_to_string(rsc_kg_edge_type_t type)
{
    switch (type) {
        case RSC_KG_EDGE_TRANSFORMS_TO:      return "transforms_to";
        case RSC_KG_EDGE_BINDS_TO:           return "binds_to";
        case RSC_KG_EDGE_USES:               return "uses";
        case RSC_KG_EDGE_CALIBRATES:         return "calibrates";
        case RSC_KG_EDGE_ANCHORS:            return "anchors";
        case RSC_KG_EDGE_ASSOCIATES_WITH:    return "associates_with";
        case RSC_KG_EDGE_PROJECTS_TO:        return "projects_to";
        case RSC_KG_EDGE_ENCODES:            return "encodes";
        case RSC_KG_EDGE_INTEGRATES:         return "integrates";
        default:                             return "unknown";
    }
}
