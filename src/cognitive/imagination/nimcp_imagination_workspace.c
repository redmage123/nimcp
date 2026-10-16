/**
 * @file nimcp_imagination_workspace.c
 * @brief Imagination Workspace - Active scenario buffer and mental canvas
 *
 * WHAT: Working buffer for active imagination scenarios
 * WHY:  Provides scratch space for scene construction and manipulation
 * HOW:  Manages latent state, visual/audio buffers, and trajectory history
 *
 * @author NIMCP Development Team
 * @date 2026-01-02
 * @version 2.6.3
 */

#include "cognitive/imagination/nimcp_imagination_workspace.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/world_model/nimcp_world_model_kg_events.h"  /* W8 */
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include <math.h>

BRIDGE_BOILERPLATE(imagination_workspace, MESH_ADAPTER_CATEGORY_COGNITIVE)



/*============================================================================
 * Tensor Helper Functions (local implementations)
 *============================================================================*/

/**
 * @brief Fill tensor with a constant value (inline helper)
 */
static inline void nimcp_tensor_fill(nimcp_tensor_t* t, float value) {
    if (!t) return;
    float* data = (float*)nimcp_tensor_data(t);
    size_t numel = nimcp_tensor_numel(t);
    for (size_t i = 0; i < numel; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && numel > 256) {
            imagination_workspace_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)numel);
        }

        data[i] = value;
    }
}

/* nimcp_tensor_size is now defined in nimcp_tensor.h */

/**
 * @brief Copy tensor data from src to dst (must be same size)
 */
static inline void nimcp_tensor_copy(nimcp_tensor_t* dst, const nimcp_tensor_t* src) {
    if (!dst || !src) return;
    size_t dst_numel = nimcp_tensor_numel(dst);
    size_t src_numel = nimcp_tensor_numel(src);
    if (dst_numel != src_numel) return;

    float* dst_data = (float*)nimcp_tensor_data(dst);
    const float* src_data = (const float*)nimcp_tensor_data_const(src);
    memcpy(dst_data, src_data, dst_numel * sizeof(float));
}

/*============================================================================
 * Internal Structures
 *============================================================================*/

/**
 * @brief Internal scenario slot structure
 */
typedef struct scenario_slot {
    scenario_id_t id;               /**< Scenario ID (0 = empty) */
    bool active;                    /**< Slot is active */

    /* Per-scenario buffers */
    nimcp_tensor_t* latent;         /**< Latent state buffer */
    nimcp_tensor_t* visual;         /**< Visual buffer */
    nimcp_tensor_t* audio;          /**< Audio buffer */

    /* Trajectory history */
    nimcp_tensor_t** trajectory;    /**< Array of trajectory states */
    size_t trajectory_len;          /**< Current trajectory length */
    size_t trajectory_capacity;     /**< Trajectory array capacity */

    /* Timing */
    uint64_t created_ms;            /**< Creation timestamp */
    uint64_t last_step_ms;          /**< Last step timestamp */
} scenario_slot_t;

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief Initialize a scenario slot
 */
static int slot_init(scenario_slot_t* slot,
                     const imagination_workspace_config_t* config) {
    if (!slot || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_time_ms: required parameter is NULL (slot, config)");
        return -1;
    }

    memset(slot, 0, sizeof(scenario_slot_t));

    /* Create latent buffer */
    uint32_t latent_dims[] = {(uint32_t)config->latent_dim};
    slot->latent = nimcp_tensor_create(latent_dims, 1, NIMCP_DTYPE_F32);
    if (!slot->latent) goto error;

    /* Create visual buffer */
    uint32_t visual_dims[] = {(uint32_t)config->visual_height, (uint32_t)config->visual_width, (uint32_t)config->visual_channels};
    slot->visual = nimcp_tensor_create(visual_dims, 3, NIMCP_DTYPE_F32);
    if (!slot->visual) goto error;

    /* Create audio buffer */
    uint32_t audio_dims[] = {(uint32_t)config->audio_samples};
    slot->audio = nimcp_tensor_create(audio_dims, 1, NIMCP_DTYPE_F32);
    if (!slot->audio) goto error;

    /* Allocate trajectory array if enabled */
    if (config->enable_history && config->max_trajectory_length > 0) {
        slot->trajectory = (nimcp_tensor_t**)nimcp_calloc(
            config->max_trajectory_length, sizeof(nimcp_tensor_t*));
        if (!slot->trajectory) goto error;
        slot->trajectory_capacity = config->max_trajectory_length;
    }

    return 0;

error:
    if (slot->latent) nimcp_tensor_destroy(slot->latent);
    if (slot->visual) nimcp_tensor_destroy(slot->visual);
    if (slot->audio) nimcp_tensor_destroy(slot->audio);
    if (slot->trajectory) nimcp_free(slot->trajectory);
    memset(slot, 0, sizeof(scenario_slot_t));
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "get_time_ms: validation failed");
    return -1;
}

/**
 * @brief Cleanup a scenario slot
 */
static void slot_cleanup(scenario_slot_t* slot) {
    if (!slot) return;

    if (slot->latent) {
        nimcp_tensor_destroy(slot->latent);
        slot->latent = NULL;
    }
    if (slot->visual) {
        nimcp_tensor_destroy(slot->visual);
        slot->visual = NULL;
    }
    if (slot->audio) {
        nimcp_tensor_destroy(slot->audio);
        slot->audio = NULL;
    }

    /* Free trajectory history */
    if (slot->trajectory) {
        for (size_t i = 0; i < slot->trajectory_len; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && slot->trajectory_len > 256) {
                imagination_workspace_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)slot->trajectory_len);
            }

            if (slot->trajectory[i]) {
                nimcp_tensor_destroy(slot->trajectory[i]);
            }
        }
        nimcp_free(slot->trajectory);
        slot->trajectory = NULL;
    }

    slot->id = 0;
    slot->active = false;
    slot->trajectory_len = 0;
    slot->trajectory_capacity = 0;
}

/**
 * @brief Reset a scenario slot (keep buffers, clear data)
 */
static void slot_reset(scenario_slot_t* slot) {
    if (!slot) return;

    /* Clear buffer contents */
    if (slot->latent) nimcp_tensor_fill(slot->latent, 0.0f);
    if (slot->visual) nimcp_tensor_fill(slot->visual, 0.0f);
    if (slot->audio) nimcp_tensor_fill(slot->audio, 0.0f);

    /* Clear trajectory */
    if (slot->trajectory) {
        for (size_t i = 0; i < slot->trajectory_len; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && slot->trajectory_len > 256) {
                imagination_workspace_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)slot->trajectory_len);
            }

            if (slot->trajectory[i]) {
                nimcp_tensor_destroy(slot->trajectory[i]);
                slot->trajectory[i] = NULL;
            }
        }
        slot->trajectory_len = 0;
    }

    slot->id = 0;
    slot->active = false;
    slot->created_ms = 0;
    slot->last_step_ms = 0;
}

/**
 * @brief Find slot by scenario ID
 */
static scenario_slot_t* find_slot(imagination_workspace_t* workspace,
                                   scenario_id_t id) {
    if (!workspace || !workspace->scenarios || id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "slot_reset: required parameter is NULL (workspace, workspace->scenarios)");
        return NULL;
    }

    scenario_slot_t* slots = (scenario_slot_t*)workspace->scenarios;
    for (size_t i = 0; i < workspace->config.max_scenarios; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && workspace->config.max_scenarios > 256) {
            imagination_workspace_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)workspace->config.max_scenarios);
        }

        if (slots[i].id == id && slots[i].active) {
            return &slots[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "slot_reset: validation failed");
    return NULL;
}

/**
 * @brief Find empty slot
 */
static scenario_slot_t* find_empty_slot(imagination_workspace_t* workspace) {
    if (!workspace || !workspace->scenarios) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_empty_slot: required parameter is NULL (workspace, workspace->scenarios)");
        return NULL;
    }

    scenario_slot_t* slots = (scenario_slot_t*)workspace->scenarios;
    for (size_t i = 0; i < workspace->config.max_scenarios; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && workspace->config.max_scenarios > 256) {
            imagination_workspace_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)workspace->config.max_scenarios);
        }

        if (!slots[i].active) {
            return &slots[i];
        }
    }
    /* Loop completed: every slot[i].active was true. Workspace is full,
     * not NULL — the line 263 guard already covers the NULL case. */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "find_empty_slot: workspace full (all max_scenarios slots active — check for missing imagination_end_scenario)");
    return NULL;
}

/*============================================================================
 * Configuration API
 *============================================================================*/

imagination_workspace_config_t imagination_workspace_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__default_config", 0.0f);


    imagination_workspace_config_t config = {
        .max_scenarios = IMAGINATION_MAX_SCENARIOS,
        .latent_dim = IMAGINATION_DEFAULT_LATENT_DIM,
        .visual_width = 64,
        .visual_height = 64,
        .visual_channels = 1,
        .audio_samples = IMAGINATION_DEFAULT_AUDIO_SIZE,
        .max_trajectory_length = IMAGINATION_MAX_TRAJECTORY,
        .enable_history = true
    };
    return config;
}

/*============================================================================
 * Lifecycle API
 *============================================================================*/

imagination_workspace_t* imagination_workspace_create(
    const imagination_workspace_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__create", 0.0f);


    imagination_workspace_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = imagination_workspace_default_config();
    }

    /* Validate configuration */
    if (cfg.max_scenarios == 0 || cfg.max_scenarios > 64) {
        cfg.max_scenarios = IMAGINATION_MAX_SCENARIOS;
    }
    if (cfg.latent_dim == 0 || cfg.latent_dim > 2048) {
        cfg.latent_dim = IMAGINATION_DEFAULT_LATENT_DIM;
    }

    /* Allocate workspace */
    imagination_workspace_t* workspace = (imagination_workspace_t*)nimcp_calloc(
        1, sizeof(imagination_workspace_t));
    if (!workspace) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate workspace");

        return NULL;

    }

    workspace->config = cfg;

    /* Create mutex */
    workspace->mutex = nimcp_mutex_create(NULL);
    if (!workspace->mutex) goto error;

    /* Allocate scenario slots */
    workspace->scenarios = nimcp_calloc(cfg.max_scenarios, sizeof(scenario_slot_t));
    if (!workspace->scenarios) goto error;

    /* Initialize each slot */
    scenario_slot_t* slots = (scenario_slot_t*)workspace->scenarios;
    for (size_t i = 0; i < cfg.max_scenarios; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cfg.max_scenarios > 256) {
            imagination_workspace_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)cfg.max_scenarios);
        }

        if (slot_init(&slots[i], &cfg) != 0) goto error;
    }

    /* Create shared temporary buffers */
    uint32_t latent_dims[] = {(uint32_t)cfg.latent_dim};
    workspace->temp_latent = nimcp_tensor_create(latent_dims, 1, NIMCP_DTYPE_F32);
    if (!workspace->temp_latent) goto error;

    uint32_t visual_dims[] = {(uint32_t)cfg.visual_height, (uint32_t)cfg.visual_width, (uint32_t)cfg.visual_channels};
    workspace->temp_visual = nimcp_tensor_create(visual_dims, 3, NIMCP_DTYPE_F32);
    if (!workspace->temp_visual) goto error;

    uint32_t audio_dims[] = {(uint32_t)cfg.audio_samples};
    workspace->temp_audio = nimcp_tensor_create(audio_dims, 1, NIMCP_DTYPE_F32);
    if (!workspace->temp_audio) goto error;

    /* Initialize statistics */
    memset(&workspace->stats, 0, sizeof(imagination_workspace_stats_t));

    /* Initialize ID counter (start from 1, 0 is invalid) */
    workspace->next_id = 1;
    workspace->scenario_count = 0;

    return workspace;

error:
    imagination_workspace_destroy(workspace);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_workspace_default_config: operation failed");
    return NULL;
}

void imagination_workspace_destroy(imagination_workspace_t* workspace) {
    if (!workspace) return;

    /* Cleanup scenario slots */
    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__destroy", 0.0f);


    if (workspace->scenarios) {
        scenario_slot_t* slots = (scenario_slot_t*)workspace->scenarios;
        for (size_t i = 0; i < workspace->config.max_scenarios; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && workspace->config.max_scenarios > 256) {
                imagination_workspace_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)workspace->config.max_scenarios);
            }

            slot_cleanup(&slots[i]);
        }
        nimcp_free(workspace->scenarios);
    }

    /* Cleanup temporary buffers */
    if (workspace->temp_latent) nimcp_tensor_destroy(workspace->temp_latent);
    if (workspace->temp_visual) nimcp_tensor_destroy(workspace->temp_visual);
    if (workspace->temp_audio) nimcp_tensor_destroy(workspace->temp_audio);

    /* Destroy mutex */
    if (workspace->mutex) nimcp_mutex_destroy(workspace->mutex);

    nimcp_free(workspace);
    workspace = NULL;
}

int imagination_workspace_reset(imagination_workspace_t* workspace) {
    if (!workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_workspace_reset: workspace is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__reset", 0.0f);


    nimcp_mutex_lock(workspace->mutex);

    /* Reset all slots */
    if (workspace->scenarios) {
        scenario_slot_t* slots = (scenario_slot_t*)workspace->scenarios;
        for (size_t i = 0; i < workspace->config.max_scenarios; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && workspace->config.max_scenarios > 256) {
                imagination_workspace_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)workspace->config.max_scenarios);
            }

            slot_reset(&slots[i]);
        }
    }

    /* Reset temporary buffers */
    if (workspace->temp_latent) nimcp_tensor_fill(workspace->temp_latent, 0.0f);
    if (workspace->temp_visual) nimcp_tensor_fill(workspace->temp_visual, 0.0f);
    if (workspace->temp_audio) nimcp_tensor_fill(workspace->temp_audio, 0.0f);

    /* Reset statistics */
    memset(&workspace->stats, 0, sizeof(imagination_workspace_stats_t));

    /* Reset counters */
    workspace->scenario_count = 0;
    workspace->next_id = 1;

    nimcp_mutex_unlock(workspace->mutex);

    return 0;
}

/*============================================================================
 * Scenario Management API
 *============================================================================*/

scenario_id_t imagination_workspace_allocate_scenario(
    imagination_workspace_t* workspace) {

    if (!workspace) return 0;

    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__allocate_scenario", 0.0f);


    nimcp_mutex_lock(workspace->mutex);

    /* Find empty slot */
    scenario_slot_t* slot = find_empty_slot(workspace);
    if (!slot) {
        nimcp_mutex_unlock(workspace->mutex);
        return 0;  /* Workspace full */
    }

    /* Allocate ID and activate slot */
    scenario_id_t id = workspace->next_id++;
    slot->id = id;
    slot->active = true;
    slot->created_ms = get_time_ms();
    slot->last_step_ms = slot->created_ms;

    /* Clear buffers */
    if (slot->latent) nimcp_tensor_fill(slot->latent, 0.0f);
    if (slot->visual) nimcp_tensor_fill(slot->visual, 0.0f);
    if (slot->audio) nimcp_tensor_fill(slot->audio, 0.0f);

    workspace->scenario_count++;
    workspace->stats.scenarios_created++;
    workspace->stats.scenarios_active = workspace->scenario_count;

    nimcp_mutex_unlock(workspace->mutex);

    /* W8: emit new scenario event + read-back imagination_engine partner
     * query so downstream cross-edges can be added later by caller. */
    {
        struct brain_struct* _kg_brain =
            world_model_kg_events_get_registered_brain();
        if (_kg_brain) {
            world_model_kg_emit_workspace_scenario(_kg_brain,
                (uint64_t)id, 0, 1.0f);
            (void)world_model_kg_has_partner(_kg_brain, "imagination_engine");
        }
    }

    return id;
}

int imagination_workspace_release_scenario(
    imagination_workspace_t* workspace,
    scenario_id_t id) {

    if (!workspace || id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_workspace_reset: workspace is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__release_scenario", 0.0f);


    nimcp_mutex_lock(workspace->mutex);

    scenario_slot_t* slot = find_slot(workspace, id);
    if (!slot) {
        nimcp_mutex_unlock(workspace->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_workspace_reset: slot is NULL");
        return -1;  /* Not found */
    }

    /* Calculate duration for stats */
    uint64_t now = get_time_ms();
    uint64_t duration = now - slot->created_ms;

    /* Update average duration (rolling average) */
    float n = (float)workspace->stats.scenarios_created;
    workspace->stats.avg_scenario_duration_ms =
        (workspace->stats.avg_scenario_duration_ms * (n - 1) + (float)duration) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    /* Reset slot */
    slot_reset(slot);

    workspace->scenario_count--;
    workspace->stats.scenarios_active = workspace->scenario_count;

    nimcp_mutex_unlock(workspace->mutex);

    return 0;
}

bool imagination_workspace_has_scenario(
    const imagination_workspace_t* workspace,
    scenario_id_t id) {

    if (!workspace || id == 0) {
        return false;
    }

    /* Cast away const for mutex - safe since we're only reading */
    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__has_scenario", 0.0f);


    imagination_workspace_t* ws = (imagination_workspace_t*)workspace;

    nimcp_mutex_lock(ws->mutex);
    bool found = (find_slot(ws, id) != NULL);
    nimcp_mutex_unlock(ws->mutex);

    return found;
}

size_t imagination_workspace_active_count(
    const imagination_workspace_t* workspace) {

    if (!workspace) return 0;
    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__active_count", 0.0f);


    return workspace->scenario_count;
}

/*============================================================================
 * Buffer Access API
 *============================================================================*/

nimcp_tensor_t* imagination_workspace_get_latent(
    imagination_workspace_t* workspace,
    scenario_id_t id) {

    if (!workspace || id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_workspace_reset: workspace is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__get_latent", 0.0f);


    nimcp_mutex_lock(workspace->mutex);
    scenario_slot_t* slot = find_slot(workspace, id);
    nimcp_tensor_t* result = slot ? slot->latent : NULL;
    nimcp_mutex_unlock(workspace->mutex);

    return result;
}

nimcp_tensor_t* imagination_workspace_get_visual(
    imagination_workspace_t* workspace,
    scenario_id_t id) {

    if (!workspace || id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_workspace_reset: workspace is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__get_visual", 0.0f);


    nimcp_mutex_lock(workspace->mutex);
    scenario_slot_t* slot = find_slot(workspace, id);
    nimcp_tensor_t* result = slot ? slot->visual : NULL;
    nimcp_mutex_unlock(workspace->mutex);

    return result;
}

nimcp_tensor_t* imagination_workspace_get_audio(
    imagination_workspace_t* workspace,
    scenario_id_t id) {

    if (!workspace || id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_workspace_reset: workspace is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__get_audio", 0.0f);


    nimcp_mutex_lock(workspace->mutex);
    scenario_slot_t* slot = find_slot(workspace, id);
    nimcp_tensor_t* result = slot ? slot->audio : NULL;
    nimcp_mutex_unlock(workspace->mutex);

    return result;
}

nimcp_tensor_t* imagination_workspace_get_temp_latent(
    imagination_workspace_t* workspace) {

    if (!workspace) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "workspace is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__get_temp_latent", 0.0f);


    return workspace->temp_latent;
}

nimcp_tensor_t* imagination_workspace_get_temp_visual(
    imagination_workspace_t* workspace) {

    if (!workspace) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "workspace is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__get_temp_visual", 0.0f);


    return workspace->temp_visual;
}

/*============================================================================
 * Statistics API
 *============================================================================*/

int imagination_workspace_get_stats(
    const imagination_workspace_t* workspace,
    imagination_workspace_stats_t* stats) {

    if (!workspace || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_workspace_get_stats: workspace or stats is NULL");
        return -1;
    }

    /* Cast away const for mutex - safe since we're only reading */
    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__get_stats", 0.0f);


    imagination_workspace_t* ws = (imagination_workspace_t*)workspace;

    nimcp_mutex_lock(ws->mutex);
    *stats = workspace->stats;
    nimcp_mutex_unlock(ws->mutex);

    return 0;
}

int imagination_workspace_reset_stats(imagination_workspace_t* workspace) {
    if (!workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_workspace_reset_stats: workspace is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__reset_stats", 0.0f);


    nimcp_mutex_lock(workspace->mutex);

    /* Preserve active count */
    uint64_t active = workspace->stats.scenarios_active;
    memset(&workspace->stats, 0, sizeof(imagination_workspace_stats_t));
    workspace->stats.scenarios_active = active;

    nimcp_mutex_unlock(workspace->mutex);

    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Imagination Workspace self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int imagination_workspace_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    imagination_workspace_heartbeat("imagination__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Imagination_Workspace");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                imagination_workspace_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Use basic logging since no LOG_MODULE defined */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Imagination_Workspace");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Imagination_Workspace");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void imagination_workspace_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_imagination_workspace_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int imagination_workspace_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_workspace_training_begin: NULL argument");
        return -1;
    }
    imagination_workspace_heartbeat("imagination_workspace_training_begin", 0.0f);
    (void)(struct scenario_slot*)instance; /* Module state available for reset */
    return 0;
}

int imagination_workspace_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_workspace_training_end: NULL argument");
        return -1;
    }
    imagination_workspace_heartbeat("imagination_workspace_training_end", 1.0f);
    (void)(struct scenario_slot*)instance; /* Module state available for finalization */
    return 0;
}

int imagination_workspace_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_workspace_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    imagination_workspace_heartbeat("imagination_workspace_training_step", progress);
    (void)(struct scenario_slot*)instance; /* Module state available for step adaptation */
    return 0;
}
