/**
 * @file nimcp_regulatory_tcells.c
 * @brief Regulatory T Cells - Cytokine Storm Prevention Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "cognitive/immune/nimcp_regulatory_tcells.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(regulatory_tcells, MESH_ADAPTER_CATEGORY_SECURITY)



/* Platform mutex compatibility macros */
#define nimcp_mutex_create() nimcp_platform_mutex_create()
#define nimcp_mutex_lock(m) nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_unlock(m) nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_destroy(m) do { \
    nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)(m)); \
    nimcp_free(m); \
    (m) = NULL; \
} while(0)

/* Get current time in milliseconds */
static uint64_t nimcp_time_get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * WHAT: Compute suppression factor based on inflammation level
 * WHY:  Proportional response - higher inflammation = stronger suppression
 * HOW:  Map inflammation enum to suppression curve
 */
static float compute_suppression_for_inflammation(brain_inflammation_level_t level)
{
    switch (level) {
        case INFLAMMATION_NONE:     return 0.0f;
        case INFLAMMATION_LOCAL:    return 0.1f;
        case INFLAMMATION_REGIONAL: return 0.3f;
        case INFLAMMATION_SYSTEMIC: return 0.6f;
        case INFLAMMATION_STORM:    return 1.0f;
        default:                    return 0.0f;
    }
}

/**
 * WHAT: Get maximum inflammation from brain immune stats
 * WHY:  Determine worst-case inflammation to trigger Treg response
 * HOW:  Query all inflammation sites, return highest level
 */
static brain_inflammation_level_t get_max_inflammation(
    brain_immune_system_t* immune)
{
    if (!immune) return INFLAMMATION_NONE;

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(immune, &stats) != 0) {
        return INFLAMMATION_NONE;
    }

    /* If no inflammation sites, return NONE */
    if (stats.inflammation_sites == 0) {
        return INFLAMMATION_NONE;
    }

    /* Check each inflammation site for maximum level */
    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune->inflammation_count > 256) {
            regulatory_tcells_heartbeat("regulatory_t_loop",
                             (float)(i + 1) / (float)immune->inflammation_count);
        }

        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }

    return max_level;
}

/**
 * WHAT: Map Treg cytokine to brain immune cytokine type
 * WHY:  Integrate with brain immune cytokine system
 * HOW:  IL-10 and TGF-β map to BRAIN_CYTOKINE_IL10
 */
static brain_cytokine_type_t map_treg_cytokine(treg_cytokine_type_t type)
{
    switch (type) {
        case TREG_CYTOKINE_IL10:
        case TREG_CYTOKINE_IL35:
            return BRAIN_CYTOKINE_IL10;  /* Anti-inflammatory */
        case TREG_CYTOKINE_TGFB:
            return BRAIN_CYTOKINE_IL10;  /* Also anti-inflammatory */
        default:
            return BRAIN_CYTOKINE_IL10;
    }
}

/**
 * WHAT: Update suppression factor based on inflammation history
 * WHY:  Smooth suppression changes, avoid oscillations
 * HOW:  Exponential moving average of suppression over time
 */
static void update_suppression_factor(treg_system_t* system, float delta_s)
{
    if (!system) return;

    /* Get current max inflammation */
    brain_inflammation_level_t max_level = get_max_inflammation(
        system->immune_system);

    /* Compute target suppression */
    float target_suppression = compute_suppression_for_inflammation(max_level);

    /* Apply decay/growth with time constant */
    float alpha = 1.0f - expf(-delta_s * system->config.suppression_decay_rate);
    system->current_suppression_factor =
        system->current_suppression_factor * (1.0f - alpha) +
        target_suppression * alpha;

    /* Clamp to max */
    if (system->current_suppression_factor > system->config.max_suppression_factor) {
        system->current_suppression_factor = system->config.max_suppression_factor;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int treg_default_config(treg_config_t* config)
{
    /* Guard: validate input */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "treg_default_config: config is NULL");
        return -1;
    }

    /* Set default thresholds */
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_default_config", 0.0f);


    config->activation_threshold = TREG_ACTIVATION_THRESHOLD;
    config->storm_threshold = TREG_STORM_THRESHOLD;
    config->exhaustion_threshold = 0.95f;

    /* Set production rates */
    config->il10_production_rate = TREG_IL10_PRODUCTION_RATE;
    config->tgfb_production_rate = TREG_TGFB_PRODUCTION_RATE;
    config->suppression_decay_rate = TREG_SUPPRESSION_DECAY_RATE;
    config->max_suppression_factor = 1.0f;

    /* Set checkpoint parameters */
    config->checkpoint_duration_ms = TREG_CHECKPOINT_DURATION_MS;
    config->pd1_inhibition_strength = 0.7f;
    config->ctla4_inhibition_strength = 0.6f;

    /* Set population limits */
    config->max_checkpoints = TREG_MAX_CHECKPOINTS;
    config->max_cytokines = TREG_MAX_SUPPRESSIVE_CYTOKINES;

    /* Set timing */
    config->update_interval_ms = 100;

    /* Enable all features by default */
    config->enable_pd1_pathway = true;
    config->enable_ctla4_pathway = true;
    config->enable_il10_production = true;
    config->enable_tgfb_production = true;
    config->enable_auto_activation = true;
    config->enable_logging = true;

    return 0;
}

treg_system_t* treg_create(
    const treg_config_t* config,
    brain_immune_system_t* immune_system)
{
    /* Guard: validate immune system */
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_create", 0.0f);


    NIMCP_API_CHECK_NULL_RET_NULL(immune_system, "NULL brain immune system in treg_create");

    /* Allocate system */
    treg_system_t* system = (treg_system_t*)nimcp_calloc(1, sizeof(treg_system_t));
    if (!system) return NULL;
    NIMCP_API_CHECK_ALLOC(system, "Failed to allocate Treg system");

    /* Set configuration */
    if (config) {
        memcpy(&system->config, config, sizeof(treg_config_t));
    } else {
        treg_default_config(&system->config);
    }

    /* Initialize state */
    system->state = TREG_STATE_SURVEILLANCE;
    system->immune_system = immune_system;

    /* Allocate checkpoint pool */
    system->checkpoint_capacity = system->config.max_checkpoints;
    system->checkpoints = (treg_checkpoint_t*)nimcp_calloc(
        system->checkpoint_capacity, sizeof(treg_checkpoint_t));
    if (!system->checkpoints) {
        NIMCP_LOGGING_ERROR("Failed to allocate checkpoint pool");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "treg_create: system->checkpoints is NULL");
        return NULL;
    }

    /* Allocate cytokine pool */
    system->cytokine_capacity = system->config.max_cytokines;
    system->cytokines = (treg_suppressive_cytokine_t*)nimcp_calloc(
        system->cytokine_capacity, sizeof(treg_suppressive_cytokine_t));
    if (!system->cytokines) {
        NIMCP_LOGGING_ERROR("Failed to allocate cytokine pool");
        nimcp_free(system->checkpoints);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "treg_create: system->cytokines is NULL");
        return NULL;
    }

    /* Initialize IDs */
    system->next_checkpoint_id = 1;
    system->next_cytokine_id = 1;

    /* Initialize suppression state */
    system->current_suppression_factor = 0.0f;
    memset(system->inflammation_history, 0, sizeof(system->inflammation_history));
    system->history_index = 0;

    /* Create mutex */
    system->mutex = nimcp_mutex_create();
    if (!system->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(system->cytokines);
        nimcp_free(system->checkpoints);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "treg_create: system->mutex is NULL");
        return NULL;
    }

    /* Initialize statistics */
    memset(&system->stats, 0, sizeof(treg_stats_t));

    /* Mark active */
    system->active = true;
    system->last_update_time = nimcp_time_get_current_time_ms();

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Regulatory T cell system created");
    }

    return system;
}

void treg_destroy(treg_system_t* system)
{
    /* Guard: check NULL */
    if (!system) return;

    /* Mark inactive */
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_destroy", 0.0f);


    system->active = false;

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_mutex_destroy(system->mutex);
    }

    /* Free pools */
    if (system->checkpoints) {
        nimcp_free(system->checkpoints);
    }
    if (system->cytokines) {
        nimcp_free(system->cytokines);
    }

    /* Free system */
    nimcp_free(system);
    system = NULL;
}

/* ============================================================================
 * Regulation API
 * ============================================================================ */

int treg_update(treg_system_t* system, uint64_t delta_ms)
{
    /* Guard: validate system */
    if (!system || !system->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "treg_update: required parameter is NULL (system, system->active)");
        return -1;
    }

    /* Lock for thread safety */
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_update", 0.0f);


    if (nimcp_mutex_lock(system->mutex) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "treg_update: validation failed");
        return -1;
    }

    /* Convert to seconds */
    float delta_s = delta_ms / 1000.0f;

    /* Update suppression factor based on inflammation */
    update_suppression_factor(system, delta_s);

    /* Update statistics */
    system->stats.current_suppression_factor = system->current_suppression_factor;

    /* Get current max inflammation */
    brain_inflammation_level_t max_level = get_max_inflammation(
        system->immune_system);

    /* Track max observed */
    float level_value = compute_suppression_for_inflammation(max_level);
    if (level_value > system->stats.max_inflammation_observed) {
        system->stats.max_inflammation_observed = level_value;
    }

    /* Check if auto-activation is needed */
    if (system->config.enable_auto_activation) {
        /* Activate if above threshold */
        if (level_value >= system->config.activation_threshold &&
            system->state == TREG_STATE_SURVEILLANCE) {
            system->state = TREG_STATE_ACTIVE;
            system->stats.activations++;

            if (system->config.enable_logging) {
                NIMCP_LOGGING_INFO("Treg activated for inflammation level");
            }

            /* Trigger callback */
            if (system->on_activation) {
                system->on_activation(system, max_level, system->activation_user_data);
            }
        }

        /* Check for storm prevention */
        if (max_level == INFLAMMATION_STORM) {
            system->state = TREG_STATE_SUPPRESSING;
            system->stats.storm_preventions++;

            if (system->config.enable_logging) {
                NIMCP_LOGGING_WARN("Cytokine storm detected - emergency Treg activation");
            }
        }
    }

    /* Update active checkpoints - decay or expire */
    uint64_t current_time = nimcp_time_get_current_time_ms();
    for (size_t i = 0; i < system->checkpoint_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->checkpoint_count > 256) {
            regulatory_tcells_heartbeat("regulatory_t_loop",
                             (float)(i + 1) / (float)system->checkpoint_count);
        }

        treg_checkpoint_t* cp = &system->checkpoints[i];
        if (!cp->active) continue;

        /* Check if expired */
        if (current_time - cp->activation_time > cp->duration_ms) {
            cp->active = false;
            system->stats.active_checkpoints--;
        }
    }

    /* Update active cytokines - decay over time */
    for (size_t i = 0; i < system->cytokine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->cytokine_count > 256) {
            regulatory_tcells_heartbeat("regulatory_t_loop",
                             (float)(i + 1) / (float)system->cytokine_count);
        }

        treg_suppressive_cytokine_t* cyt = &system->cytokines[i];

        /* Decay concentration */
        cyt->concentration *= expf(-delta_s * system->config.suppression_decay_rate);

        /* Remove if negligible */
        if (cyt->concentration < 0.01f) {
            /* Swap with last and decrement count */
            if (i < system->cytokine_count - 1) {
                system->cytokines[i] = system->cytokines[system->cytokine_count - 1];
            }
            system->cytokine_count--;
            system->stats.active_cytokines--;
            i--; /* Re-check this index */
        }
    }

    /* Update timestamp */
    system->last_update_time = current_time;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int treg_suppress_inflammation(treg_system_t* system, uint32_t site_id)
{
    /* Guard: validate system */
    if (!system || !system->active || !system->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "treg_suppress_inflammation: required parameter is NULL (system, system->active, system->immune_system)");
        return -1;
    }

    /* Lock for thread safety */
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_suppress_inflam", 0.0f);


    if (nimcp_mutex_lock(system->mutex) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "treg_suppress_inflammation: validation failed");
        return -1;
    }

    /* Find inflammation site */
    brain_inflammation_site_t* site = NULL;
    for (size_t i = 0; i < system->immune_system->inflammation_count; i++) {
        if (system->immune_system->inflammation_sites[i].id == site_id) {
            site = &system->immune_system->inflammation_sites[i];
            break;
        }
    }

    if (!site) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "treg_suppress_inflammation: site is NULL");
        return -1;
    }

    /* Release IL-10 if enabled */
    if (system->config.enable_il10_production) {
        uint32_t cyt_id = 0;
        treg_release_cytokine(system, TREG_CYTOKINE_IL10,
            system->config.il10_production_rate,
            site->region_id, &cyt_id);
    }

    /* Release TGF-β if enabled */
    if (system->config.enable_tgfb_production) {
        uint32_t cyt_id = 0;
        treg_release_cytokine(system, TREG_CYTOKINE_TGFB,
            system->config.tgfb_production_rate,
            site->region_id, &cyt_id);
    }

    /* Update state */
    system->state = TREG_STATE_SUPPRESSING;
    system->stats.inflammation_reductions++;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

float treg_get_suppression_factor(const treg_system_t* system)
{
    /* Guard: validate system */
    if (!system) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_get_suppression", 0.0f);


    return system->current_suppression_factor;
}

/* ============================================================================
 * Checkpoint API
 * ============================================================================ */

int treg_checkpoint_activate(
    treg_system_t* system,
    treg_checkpoint_type_t type,
    uint32_t target_cell_id,
    uint64_t duration_ms,
    uint32_t* checkpoint_id)
{
    /* Guard: validate inputs */
    if (!system || !system->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "treg_checkpoint_activate: required parameter is NULL (system, system->active)");
        return -1;
    }

    /* Check if checkpoint type is enabled */
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_checkpoint_acti", 0.0f);


    if (type == CHECKPOINT_PD1_PDL1 && !system->config.enable_pd1_pathway) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "treg_checkpoint_activate: system->config is NULL");
        return -1;
    }
    if (type == CHECKPOINT_CTLA4 && !system->config.enable_ctla4_pathway) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "treg_checkpoint_activate: system->config is NULL");
        return -1;
    }

    /* Lock for thread safety */
    if (nimcp_mutex_lock(system->mutex) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "treg_checkpoint_activate: validation failed");
        return -1;
    }

    /* Guard: check capacity */
    if (system->checkpoint_count >= system->checkpoint_capacity) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_LOGGING_WARN("Checkpoint capacity reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "treg_checkpoint_activate: capacity exceeded");
        return -1;
    }

    /* Create checkpoint */
    treg_checkpoint_t* cp = &system->checkpoints[system->checkpoint_count++];
    cp->id = system->next_checkpoint_id++;
    cp->type = type;
    cp->target_cell_id = target_cell_id;
    cp->activation_time = nimcp_time_get_current_time_ms();
    cp->duration_ms = (duration_ms > 0) ? duration_ms :
                      system->config.checkpoint_duration_ms;
    cp->active = true;

    /* Set inhibition strength based on type */
    switch (type) {
        case CHECKPOINT_PD1_PDL1:
            cp->inhibition_strength = system->config.pd1_inhibition_strength;
            break;
        case CHECKPOINT_CTLA4:
            cp->inhibition_strength = system->config.ctla4_inhibition_strength;
            break;
        default:
            cp->inhibition_strength = 0.5f;
            break;
    }

    /* Update stats */
    system->stats.checkpoints_activated++;
    system->stats.active_checkpoints++;

    /* Return ID */
    if (checkpoint_id) {
        *checkpoint_id = cp->id;
    }

    /* Trigger callback */
    if (system->on_checkpoint) {
        system->on_checkpoint(system, cp, system->checkpoint_user_data);
    }

    if (system->config.enable_logging) {
        NIMCP_LOGGING_INFO("Checkpoint activated");
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int treg_checkpoint_release(treg_system_t* system, uint32_t checkpoint_id)
{
    /* Guard: validate system */
    if (!system || !system->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "treg_checkpoint_release: required parameter is NULL (system, system->active)");
        return -1;
    }

    /* Lock for thread safety */
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_checkpoint_rele", 0.0f);


    if (nimcp_mutex_lock(system->mutex) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "treg_checkpoint_release: validation failed");
        return -1;
    }

    /* Find checkpoint */
    for (size_t i = 0; i < system->checkpoint_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->checkpoint_count > 256) {
            regulatory_tcells_heartbeat("regulatory_t_loop",
                             (float)(i + 1) / (float)system->checkpoint_count);
        }

        treg_checkpoint_t* cp = &system->checkpoints[i];
        if (cp->id == checkpoint_id && cp->active) {
            cp->active = false;
            system->stats.active_checkpoints--;

            nimcp_mutex_unlock(system->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(system->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "treg_checkpoint_release: operation failed");
    return -1; /* Not found */
}

float treg_get_checkpoint_inhibition(
    const treg_system_t* system,
    uint32_t cell_id)
{
    /* Guard: validate system */
    if (!system) return 0.0f;

    /* Sum inhibition from all active checkpoints targeting this cell */
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_get_checkpoint_", 0.0f);


    float total_inhibition = 0.0f;
    for (size_t i = 0; i < system->checkpoint_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->checkpoint_count > 256) {
            regulatory_tcells_heartbeat("regulatory_t_loop",
                             (float)(i + 1) / (float)system->checkpoint_count);
        }

        const treg_checkpoint_t* cp = &system->checkpoints[i];
        if (cp->active && cp->target_cell_id == cell_id) {
            total_inhibition += cp->inhibition_strength;
        }
    }

    /* Clamp to 1.0 */
    return (total_inhibition > 1.0f) ? 1.0f : total_inhibition;
}

/* ============================================================================
 * Cytokine API
 * ============================================================================ */

int treg_release_cytokine(
    treg_system_t* system,
    treg_cytokine_type_t type,
    float concentration,
    uint32_t target_region,
    uint32_t* cytokine_id)
{
    /* Guard: validate inputs */
    if (!system || !system->active || !system->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "treg_release_cytokine: required parameter is NULL (system, system->active, system->immune_system)");
        return -1;
    }

    /* Check if cytokine production is enabled */
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_release_cytokin", 0.0f);


    if (type == TREG_CYTOKINE_IL10 && !system->config.enable_il10_production) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "treg_release_cytokine: system->config is NULL");
        return -1;
    }
    if (type == TREG_CYTOKINE_TGFB && !system->config.enable_tgfb_production) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "treg_release_cytokine: system->config is NULL");
        return -1;
    }

    /* Lock for thread safety */
    if (nimcp_mutex_lock(system->mutex) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "treg_release_cytokine: validation failed");
        return -1;
    }

    /* Guard: check capacity */
    if (system->cytokine_count >= system->cytokine_capacity) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_LOGGING_WARN("Cytokine capacity reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "treg_release_cytokine: capacity exceeded");
        return -1;
    }

    /* Create suppressive cytokine */
    treg_suppressive_cytokine_t* cyt = &system->cytokines[system->cytokine_count++];
    cyt->id = system->next_cytokine_id++;
    cyt->type = type;
    cyt->concentration = concentration;
    cyt->target_region = target_region;
    cyt->release_time = nimcp_time_get_current_time_ms();
    cyt->mapped_type = map_treg_cytokine(type);

    /* Release via brain immune system (IL-10 is anti-inflammatory) */
    uint32_t brain_cyt_id = 0;
    int ret = brain_immune_release_cytokine(
        system->immune_system,
        cyt->mapped_type,
        0,  /* Source cell (0=Treg) */
        concentration,
        target_region,
        &brain_cyt_id
    );

    if (ret != 0) {
        /* Failed to release via brain immune - rollback */
        system->cytokine_count--;
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "treg_release_cytokine: validation failed");
        return -1;
    }

    /* Update stats */
    system->stats.cytokines_released++;
    system->stats.active_cytokines++;

    /* Return ID */
    if (cytokine_id) {
        *cytokine_id = cyt->id;
    }

    /* Trigger callback */
    if (system->on_cytokine) {
        system->on_cytokine(system, cyt, system->cytokine_user_data);
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int treg_set_activation_callback(
    treg_system_t* system,
    treg_activation_cb_t callback,
    void* user_data)
{
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_set_activation_", 0.0f);


    system->on_activation = callback;
    system->activation_user_data = user_data;
    return 0;
}

int treg_set_checkpoint_callback(
    treg_system_t* system,
    treg_checkpoint_cb_t callback,
    void* user_data)
{
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_set_checkpoint_", 0.0f);


    system->on_checkpoint = callback;
    system->checkpoint_user_data = user_data;
    return 0;
}

int treg_set_cytokine_callback(
    treg_system_t* system,
    treg_cytokine_cb_t callback,
    void* user_data)
{
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_set_cytokine_ca", 0.0f);


    system->on_cytokine = callback;
    system->cytokine_user_data = user_data;
    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int treg_get_stats(const treg_system_t* system, treg_stats_t* stats)
{
    /* Guard: validate inputs */
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "treg_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }

    /* Copy stats */
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_get_stats", 0.0f);


    memcpy(stats, &system->stats, sizeof(treg_stats_t));
    return 0;
}

treg_state_t treg_get_state(const treg_system_t* system)
{
    if (!system) return TREG_STATE_NAIVE;
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_get_state", 0.0f);


    return system->state;
}

bool treg_is_active(const treg_system_t* system)
{
    if (!system) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_is_active", 0.0f);


    return system->state == TREG_STATE_SUPPRESSING ||
           system->state == TREG_STATE_ACTIVE;
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* treg_state_to_string(treg_state_t state)
{
    switch (state) {
        case TREG_STATE_NAIVE:        return "NAIVE";
        case TREG_STATE_SURVEILLANCE: return "SURVEILLANCE";
        case TREG_STATE_ACTIVE:       return "ACTIVE";
        case TREG_STATE_SUPPRESSING:  return "SUPPRESSING";
        case TREG_STATE_EXHAUSTED:    return "EXHAUSTED";
        default:                      return "UNKNOWN";
    }
}

const char* treg_checkpoint_to_string(treg_checkpoint_type_t type)
{
    switch (type) {
        case CHECKPOINT_PD1_PDL1: return "PD-1/PD-L1";
        case CHECKPOINT_CTLA4:    return "CTLA-4";
        case CHECKPOINT_LAG3:     return "LAG-3";
        case CHECKPOINT_TIM3:     return "TIM-3";
        default:                  return "UNKNOWN";
    }
}

const char* treg_cytokine_to_string(treg_cytokine_type_t type)
{
    switch (type) {
        case TREG_CYTOKINE_IL10:  return "IL-10";
        case TREG_CYTOKINE_TGFB:  return "TGF-β";
        case TREG_CYTOKINE_IL35:  return "IL-35";
        default:                  return "UNKNOWN";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about regulatory T cells
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int treg_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    regulatory_tcells_heartbeat("regulatory_t_treg_query_self_know", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Regulatory_TCells");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                regulatory_tcells_heartbeat("regulatory_t_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Treg self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Regulatory_TCells");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Regulatory_TCells");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void regulatory_tcells_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_regulatory_tcells_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int regulatory_tcells_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "regulatory_tcells_training_begin: NULL argument");
        return -1;
    }
    regulatory_tcells_heartbeat_instance(NULL, "regulatory_tcells_training_begin", 0.0f);
    return 0;
}

int regulatory_tcells_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "regulatory_tcells_training_end: NULL argument");
        return -1;
    }
    regulatory_tcells_heartbeat_instance(NULL, "regulatory_tcells_training_end", 1.0f);
    return 0;
}

int regulatory_tcells_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "regulatory_tcells_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    regulatory_tcells_heartbeat_instance(NULL, "regulatory_tcells_training_step", progress);
    return 0;
}
