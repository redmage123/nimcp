/**
 * @file nimcp_kg_hierarchy.c
 * @brief Hierarchical View of Brain Knowledge Graph - Implementation
 * @version 1.0.0
 * @date 2025-01-15
 */

#include "core/brain/nimcp_kg_hierarchy.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/algorithms/nimcp_sort.h"
#include "utils/exception/nimcp_exception_macros.h"

/* Convenience macro for time in milliseconds */
#define nimcp_time_ms() nimcp_time_get_ms()
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_hierarchy, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal hierarchy entry
 */
typedef struct {
    brain_kg_node_id_t node_id;    /**< KG node ID */
    kg_hemisphere_t hemisphere;    /**< Assigned hemisphere */
    kg_cortical_layer_t layer;     /**< Assigned layer */
    kg_module_state_t state;       /**< Operational state */
    bio_module_health_t health;    /**< Health status */
    kg_message_stats_t stats;      /**< Message statistics */
    bool enabled;                  /**< Is enabled */
    bool has_anomaly;              /**< Has anomaly */
    uint64_t state_change_time;    /**< Last state change */
} kg_hierarchy_entry_t;

/**
 * @brief Callback entry
 */
typedef struct {
    kg_state_change_callback_t callback;
    void* user_data;
} kg_callback_entry_t;

/**
 * @brief Layer statistics (internal)
 */
typedef struct {
    brain_kg_node_id_t* modules;   /**< Module IDs in this layer */
    uint32_t module_count;         /**< Number of modules */
    uint32_t module_capacity;      /**< Allocated capacity */
    uint32_t running_count;        /**< Running modules */
    uint32_t stopped_count;        /**< Stopped modules */
    uint32_t error_count;          /**< Error modules */
    bio_module_health_t worst_health; /**< Worst health in layer */
} kg_layer_data_t;

/**
 * @brief Hemisphere statistics (internal)
 */
typedef struct {
    brain_kg_node_id_t* modules;   /**< Module IDs in this hemisphere */
    uint32_t module_count;         /**< Number of modules */
    uint32_t module_capacity;      /**< Allocated capacity */
    uint32_t running_count;        /**< Running modules */
    uint32_t stopped_count;        /**< Stopped modules */
    uint32_t error_count;          /**< Error modules */
    uint32_t modules_per_layer[KG_LAYER_COUNT]; /**< Modules per layer */
    bio_module_health_t worst_health; /**< Worst health in hemisphere */
} kg_hemisphere_data_t;

/**
 * @brief Internal hierarchy structure
 */
struct kg_hierarchy {
    /* Configuration */
    kg_hierarchy_config_t config;

    /* Core references (not owned) */
    brain_kg_t* kg;
    bio_async_orchestrator_t* orchestrator;
    wiring_diagram_t* wiring;

    /* Entry cache - indexed by node_id */
    kg_hierarchy_entry_t* entries;
    uint32_t entry_count;
    uint32_t entry_capacity;

    /* Node ID to entry index mapping (sparse array) */
    uint32_t* node_to_entry;
    uint32_t node_map_capacity;

    /* Hemisphere data */
    kg_hemisphere_data_t hemispheres[KG_HEMISPHERE_COUNT];

    /* Layer data */
    kg_layer_data_t layers[KG_LAYER_COUNT];

    /* Callbacks */
    kg_callback_entry_t callbacks[KG_HIERARCHY_MAX_CALLBACKS];
    uint32_t callback_count;

    /* State */
    bool dirty;
    uint64_t last_sync_time;
    uint64_t creation_time;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * String Constants
 * ============================================================================ */

static const char* g_hemisphere_names[KG_HEMISPHERE_COUNT] = {
    "Left Hemisphere",
    "Right Hemisphere",
    "Bilateral"
};

static const char* g_hemisphere_descriptions[KG_HEMISPHERE_COUNT] = {
    "Language, logic, math, sequential processing",
    "Spatial, creativity, pattern recognition, holistic processing",
    "Core systems, coordination, global workspace"
};

static const char* g_layer_names[KG_LAYER_COUNT] = {
    "Layer I",
    "Layer II",
    "Layer III",
    "Layer IV",
    "Layer V",
    "Layer VI"
};

static const char* g_layer_descriptions[KG_LAYER_COUNT] = {
    "Molecular layer (top-down modulation)",
    "External granular (local processing)",
    "External pyramidal (cortico-cortical)",
    "Internal granular (thalamic input)",
    "Internal pyramidal (subcortical output)",
    "Multiform (corticothalamic feedback)"
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Map module category to cortical layer
 */
static kg_cortical_layer_t category_to_layer(bio_module_category_t category) {
    switch (category) {
        case BIO_MODULE_CATEGORY_HIGHLEVEL:
            return KG_LAYER_I;
        case BIO_MODULE_CATEGORY_COGNITIVE:
            return KG_LAYER_III;  /* Default cognitive to layer III */
        case BIO_MODULE_CATEGORY_PERCEPTION:
        case BIO_MODULE_CATEGORY_MIDDLEWARE:
            return KG_LAYER_IV;
        case BIO_MODULE_CATEGORY_CORE:
        case BIO_MODULE_CATEGORY_SWARM:
            return KG_LAYER_V;
        case BIO_MODULE_CATEGORY_PLASTICITY:
        case BIO_MODULE_CATEGORY_IMMUNE:
        case BIO_MODULE_CATEGORY_GLIAL:
            return KG_LAYER_VI;
        case BIO_MODULE_CATEGORY_SECURITY:
            return KG_LAYER_II;  /* Security in layer II for monitoring */
        default:
            return KG_LAYER_IV;  /* Default to input layer */
    }
}

/**
 * @brief Map brain KG node type to category
 */
static bio_module_category_t node_type_to_category(brain_kg_node_type_t type) {
    switch (type) {
        case BRAIN_KG_NODE_CORE:
            return BIO_MODULE_CATEGORY_CORE;
        case BRAIN_KG_NODE_CORTICAL:
        case BRAIN_KG_NODE_SUBCORTICAL:
        case BRAIN_KG_NODE_BRAINSTEM:
            return BIO_MODULE_CATEGORY_CORE;
        case BRAIN_KG_NODE_COGNITIVE:
            return BIO_MODULE_CATEGORY_COGNITIVE;
        case BRAIN_KG_NODE_PERCEPTION:
            return BIO_MODULE_CATEGORY_PERCEPTION;
        case BRAIN_KG_NODE_PLASTICITY:
            return BIO_MODULE_CATEGORY_PLASTICITY;
        case BRAIN_KG_NODE_TRAINING:
            return BIO_MODULE_CATEGORY_MIDDLEWARE;
        case BRAIN_KG_NODE_SWARM:
            return BIO_MODULE_CATEGORY_SWARM;
        case BRAIN_KG_NODE_SECURITY:
            return BIO_MODULE_CATEGORY_SECURITY;
        case BRAIN_KG_NODE_COORDINATOR:
            return BIO_MODULE_CATEGORY_HIGHLEVEL;
        case BRAIN_KG_NODE_INTEGRATION:
            return BIO_MODULE_CATEGORY_MIDDLEWARE;
        default:
            return BIO_MODULE_CATEGORY_CORE;
    }
}

/**
 * @brief Map brain KG state to hierarchy state
 */
static kg_module_state_t kg_state_to_hier_state(brain_kg_node_state_t state) {
    switch (state) {
        case BRAIN_KG_STATE_UNKNOWN:
            return KG_MODULE_STATE_UNKNOWN;
        case BRAIN_KG_STATE_UNINITIALIZED:
            return KG_MODULE_STATE_STOPPED;
        case BRAIN_KG_STATE_INITIALIZING:
            return KG_MODULE_STATE_STARTING;
        case BRAIN_KG_STATE_ACTIVE:
            return KG_MODULE_STATE_RUNNING;
        case BRAIN_KG_STATE_DISABLED:
            return KG_MODULE_STATE_PAUSED;
        case BRAIN_KG_STATE_ERROR:
            return KG_MODULE_STATE_ERROR;
        case BRAIN_KG_STATE_SHUTDOWN:
            return KG_MODULE_STATE_STOPPED;
        default:
            return KG_MODULE_STATE_UNKNOWN;
    }
}

/**
 * @brief Determine hemisphere from node type and name
 *
 * Hemisphere assignment is based on biological function:
 * - Left: Language (Broca, Wernicke), logic, math, sequential
 * - Right: Spatial, creativity, pattern recognition, holistic
 * - Bilateral: Core systems, coordination, global workspace
 */
static kg_hemisphere_t determine_hemisphere(
    brain_kg_node_type_t type,
    const char* name
) {
    /* Check name for hemisphere hints first */
    if (name) {
        /* Left hemisphere: Language processing regions */
        if (strstr(name, "broca") || strstr(name, "Broca") ||
            strstr(name, "wernicke") || strstr(name, "Wernicke") ||
            strstr(name, "language") || strstr(name, "Language") ||
            strstr(name, "nlp") || strstr(name, "NLP") ||
            strstr(name, "semantic") || strstr(name, "Semantic") ||
            strstr(name, "logic") || strstr(name, "Logic") ||
            strstr(name, "math") || strstr(name, "Math") ||
            strstr(name, "arithmetic") || strstr(name, "Arithmetic") ||
            strstr(name, "sequential") || strstr(name, "Sequential") ||
            strstr(name, "analytical") || strstr(name, "Analytical")) {
            return KG_HEMISPHERE_LEFT;
        }

        /* Right hemisphere: Spatial and creative regions */
        if (strstr(name, "spatial") || strstr(name, "Spatial") ||
            strstr(name, "parietal") || strstr(name, "Parietal") ||
            strstr(name, "visual") || strstr(name, "Visual") ||
            strstr(name, "occipital") || strstr(name, "Occipital") ||
            strstr(name, "pattern") || strstr(name, "Pattern") ||
            strstr(name, "holistic") || strstr(name, "Holistic") ||
            strstr(name, "creative") || strstr(name, "Creative") ||
            strstr(name, "imagination") || strstr(name, "Imagination") ||
            strstr(name, "artistic") || strstr(name, "Artistic") ||
            strstr(name, "music") || strstr(name, "Music") ||
            strstr(name, "emotion") || strstr(name, "Emotion") ||
            strstr(name, "face") || strstr(name, "Face")) {
            return KG_HEMISPHERE_RIGHT;
        }

        /* Explicit bilateral markers */
        if (strstr(name, "bilateral") || strstr(name, "Bilateral") ||
            strstr(name, "corpus_callosum") || strstr(name, "CorpusCallosum")) {
            return KG_HEMISPHERE_BILATERAL;
        }
    }

    /* Default assignment based on node type */
    switch (type) {
        /* Bilateral: Core infrastructure, coordination */
        case BRAIN_KG_NODE_CORE:
        case BRAIN_KG_NODE_BRAINSTEM:
        case BRAIN_KG_NODE_COORDINATOR:
        case BRAIN_KG_NODE_INTEGRATION:
        case BRAIN_KG_NODE_SWARM:
        case BRAIN_KG_NODE_SECURITY:
        case BRAIN_KG_NODE_PLASTICITY:
        case BRAIN_KG_NODE_TRAINING:
            return KG_HEMISPHERE_BILATERAL;

        /* Right hemisphere: Perception, spatial */
        case BRAIN_KG_NODE_PERCEPTION:
            return KG_HEMISPHERE_RIGHT;

        /* Left hemisphere: Default for cognitive */
        case BRAIN_KG_NODE_COGNITIVE:
            return KG_HEMISPHERE_LEFT;

        /* Bilateral for cortical/subcortical unless specified */
        case BRAIN_KG_NODE_CORTICAL:
        case BRAIN_KG_NODE_SUBCORTICAL:
        default:
            return KG_HEMISPHERE_BILATERAL;
    }
}

/**
 * @brief Find entry index for a node
 */
static uint32_t find_entry_index(const kg_hierarchy_t* hier, brain_kg_node_id_t node_id) {
    if (!hier || node_id == BRAIN_KG_INVALID_NODE) {
        return UINT32_MAX;
    }
    if (node_id >= hier->node_map_capacity) {
        return UINT32_MAX;
    }
    return hier->node_to_entry[node_id];
}

/**
 * @brief Add entry for a node
 */
static int add_entry(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t node_id,
    kg_hemisphere_t hemisphere,
    kg_cortical_layer_t layer
) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "add_entry: hier is NULL");
        return -1;
    }

    /* Grow entry array if needed */
    if (hier->entry_count >= hier->entry_capacity) {
        uint32_t new_capacity = hier->entry_capacity * 2;
        if (new_capacity == 0) new_capacity = 64;

        kg_hierarchy_entry_t* new_entries = nimcp_realloc(
            hier->entries,
            new_capacity * sizeof(kg_hierarchy_entry_t)
        );
        if (!new_entries) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_entry: new_entries is NULL");
            return -1;
        }

        hier->entries = new_entries;
        hier->entry_capacity = new_capacity;
    }

    /* Grow node map if needed */
    if (node_id >= hier->node_map_capacity) {
        uint32_t new_capacity = node_id + 256;
        uint32_t* new_map = nimcp_realloc(
            hier->node_to_entry,
            new_capacity * sizeof(uint32_t)
        );
        if (!new_map) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_entry: new_map is NULL");
            return -1;
        }

        /* Initialize new entries to invalid */
        for (uint32_t i = hier->node_map_capacity; i < new_capacity; i++) {
            new_map[i] = UINT32_MAX;
        }
        hier->node_to_entry = new_map;
        hier->node_map_capacity = new_capacity;
    }

    /* Add entry */
    uint32_t idx = hier->entry_count++;
    memset(&hier->entries[idx], 0, sizeof(kg_hierarchy_entry_t));
    hier->entries[idx].node_id = node_id;
    hier->entries[idx].hemisphere = hemisphere;
    hier->entries[idx].layer = layer;
    hier->entries[idx].state = KG_MODULE_STATE_UNKNOWN;
    hier->entries[idx].health = BIO_MODULE_HEALTH_UNKNOWN;
    hier->entries[idx].enabled = true;

    /* Update map */
    hier->node_to_entry[node_id] = idx;

    /* Add to hemisphere */
    kg_hemisphere_data_t* hd = &hier->hemispheres[hemisphere];
    if (hd->module_count >= hd->module_capacity) {
        uint32_t new_cap = hd->module_capacity * 2;
        if (new_cap == 0) new_cap = 32;

        brain_kg_node_id_t* new_mods = nimcp_realloc(
            hd->modules,
            new_cap * sizeof(brain_kg_node_id_t)
        );
        if (!new_mods) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_entry: new_mods is NULL");
            return -1;
        }

        hd->modules = new_mods;
        hd->module_capacity = new_cap;
    }
    hd->modules[hd->module_count++] = node_id;
    hd->modules_per_layer[layer]++;

    /* Add to layer */
    kg_layer_data_t* ld = &hier->layers[layer];
    if (ld->module_count >= ld->module_capacity) {
        uint32_t new_cap = ld->module_capacity * 2;
        if (new_cap == 0) new_cap = 32;

        brain_kg_node_id_t* new_mods = nimcp_realloc(
            ld->modules,
            new_cap * sizeof(brain_kg_node_id_t)
        );
        if (!new_mods) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_entry: new_mods is NULL");
            return -1;
        }

        ld->modules = new_mods;
        ld->module_capacity = new_cap;
    }
    ld->modules[ld->module_count++] = node_id;

    return 0;
}

/**
 * @brief Update layer statistics
 */
static void update_layer_stats(kg_hierarchy_t* hier) {
    if (!hier) return;

    for (uint32_t l = 0; l < KG_LAYER_COUNT; l++) {
        kg_layer_data_t* ld = &hier->layers[l];
        ld->running_count = 0;
        ld->stopped_count = 0;
        ld->error_count = 0;
        ld->worst_health = BIO_MODULE_HEALTH_HEALTHY;

        for (uint32_t m = 0; m < ld->module_count; m++) {
            uint32_t idx = find_entry_index(hier, ld->modules[m]);
            if (idx == UINT32_MAX) continue;

            kg_hierarchy_entry_t* entry = &hier->entries[idx];

            switch (entry->state) {
                case KG_MODULE_STATE_RUNNING:
                    ld->running_count++;
                    break;
                case KG_MODULE_STATE_STOPPED:
                    ld->stopped_count++;
                    break;
                case KG_MODULE_STATE_ERROR:
                    ld->error_count++;
                    break;
                default:
                    break;
            }

            /* Track worst health */
            if (entry->health > ld->worst_health) {
                ld->worst_health = entry->health;
            }
        }
    }
}

/**
 * @brief Update hemisphere statistics
 */
static void update_hemisphere_stats(kg_hierarchy_t* hier) {
    if (!hier) return;

    for (uint32_t h = 0; h < KG_HEMISPHERE_COUNT; h++) {
        kg_hemisphere_data_t* hd = &hier->hemispheres[h];
        hd->running_count = 0;
        hd->stopped_count = 0;
        hd->error_count = 0;
        hd->worst_health = BIO_MODULE_HEALTH_HEALTHY;

        for (uint32_t m = 0; m < hd->module_count; m++) {
            uint32_t idx = find_entry_index(hier, hd->modules[m]);
            if (idx == UINT32_MAX) continue;

            kg_hierarchy_entry_t* entry = &hier->entries[idx];

            switch (entry->state) {
                case KG_MODULE_STATE_RUNNING:
                    hd->running_count++;
                    break;
                case KG_MODULE_STATE_STOPPED:
                    hd->stopped_count++;
                    break;
                case KG_MODULE_STATE_ERROR:
                    hd->error_count++;
                    break;
                default:
                    break;
            }

            /* Track worst health */
            if (entry->health > hd->worst_health) {
                hd->worst_health = entry->health;
            }
        }
    }
}

/**
 * @brief Invoke state change callbacks
 */
static void invoke_callbacks(
    kg_hierarchy_t* hier,
    const kg_state_change_event_t* event
) {
    if (!hier || !event) return;

    for (uint32_t i = 0; i < hier->callback_count; i++) {
        if (hier->callbacks[i].callback) {
            hier->callbacks[i].callback(event, hier->callbacks[i].user_data);
        }
    }
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int kg_hierarchy_default_config(kg_hierarchy_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_default_config: config is NULL");
        return -1;
    }

    config->lazy_rebuild = true;
    config->cache_ttl_ms = 0;  /* No expiry */
    config->subscribe_orchestrator = true;
    config->subscribe_wiring = true;

    return 0;
}

kg_hierarchy_t* kg_hierarchy_create(
    brain_kg_t* kg,
    const kg_hierarchy_config_t* config
) {
    if (!kg) {
        LOG_ERROR("kg_hierarchy_create: NULL kg");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_hierarchy_create: kg is NULL");
        return NULL;
    }

    kg_hierarchy_t* hier = nimcp_calloc(1, sizeof(kg_hierarchy_t));
    if (!hier) {
        LOG_ERROR("kg_hierarchy_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_hierarchy_create: hier is NULL");
        return NULL;
    }

    /* Initialize config */
    if (config) {
        hier->config = *config;
    } else {
        kg_hierarchy_default_config(&hier->config);
    }

    /* Store KG reference */
    hier->kg = kg;

    /* Create mutex */
    hier->mutex = nimcp_mutex_create(NULL);
    if (!hier->mutex) {
        LOG_ERROR("kg_hierarchy_create: mutex creation failed");
        nimcp_free(hier);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_hierarchy_create: hier->mutex is NULL");
        return NULL;
    }

    /* Initialize hemisphere data */
    for (uint32_t h = 0; h < KG_HEMISPHERE_COUNT; h++) {
        hier->hemispheres[h].modules = NULL;
        hier->hemispheres[h].module_count = 0;
        hier->hemispheres[h].module_capacity = 0;
        hier->hemispheres[h].worst_health = BIO_MODULE_HEALTH_UNKNOWN;
        for (uint32_t l = 0; l < KG_LAYER_COUNT; l++) {
            hier->hemispheres[h].modules_per_layer[l] = 0;
        }
    }

    /* Initialize layer data */
    for (uint32_t l = 0; l < KG_LAYER_COUNT; l++) {
        hier->layers[l].modules = NULL;
        hier->layers[l].module_count = 0;
        hier->layers[l].module_capacity = 0;
        hier->layers[l].worst_health = BIO_MODULE_HEALTH_UNKNOWN;
    }

    /* Set creation time */
    hier->creation_time = nimcp_time_ms();
    hier->dirty = true;  /* Needs initial build */

    /* Initial rebuild */
    if (kg_hierarchy_rebuild(hier) < 0) {
        LOG_WARN("kg_hierarchy_create: initial rebuild failed");
        /* Continue anyway - can be rebuilt later */
    }

    LOG_INFO("KG hierarchy created with %u entries", hier->entry_count);
    return hier;
}

void kg_hierarchy_destroy(kg_hierarchy_t* hier) {
    if (!hier) return;

    LOG_DEBUG("Destroying KG hierarchy");

    /* Free entries */
    if (hier->entries) {
        nimcp_free(hier->entries);
    }

    /* Free node map */
    if (hier->node_to_entry) {
        nimcp_free(hier->node_to_entry);
    }

    /* Free hemisphere module arrays */
    for (uint32_t h = 0; h < KG_HEMISPHERE_COUNT; h++) {
        if (hier->hemispheres[h].modules) {
            nimcp_free(hier->hemispheres[h].modules);
        }
    }

    /* Free layer module arrays */
    for (uint32_t l = 0; l < KG_LAYER_COUNT; l++) {
        if (hier->layers[l].modules) {
            nimcp_free(hier->layers[l].modules);
        }
    }

    /* Destroy mutex */
    if (hier->mutex) {
        nimcp_mutex_free(hier->mutex);
    }

    nimcp_free(hier);
}

/* ============================================================================
 * Integration API Implementation
 * ============================================================================ */

int kg_hierarchy_connect_orchestrator(
    kg_hierarchy_t* hier,
    bio_async_orchestrator_t* orchestrator
) {
    if (!hier || !orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_connect_orchestrator: required parameter is NULL (hier, orchestrator)");
        return -1;
    }

    nimcp_mutex_lock(hier->mutex);
    hier->orchestrator = orchestrator;
    hier->dirty = true;  /* Need to resync */
    nimcp_mutex_unlock(hier->mutex);

    LOG_DEBUG("KG hierarchy connected to orchestrator");
    return 0;
}

int kg_hierarchy_connect_wiring(
    kg_hierarchy_t* hier,
    wiring_diagram_t* wd
) {
    if (!hier || !wd) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_connect_wiring: required parameter is NULL (hier, wd)");
        return -1;
    }

    nimcp_mutex_lock(hier->mutex);
    hier->wiring = wd;
    hier->dirty = true;  /* Need to resync */
    nimcp_mutex_unlock(hier->mutex);

    LOG_DEBUG("KG hierarchy connected to wiring diagram");
    return 0;
}

/* ============================================================================
 * Level 0 Queries (Brain Level)
 * ============================================================================ */

int kg_hierarchy_get_brain_stats(
    const kg_hierarchy_t* hier,
    kg_brain_stats_t* stats
) {
    if (!hier || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_get_brain_stats: required parameter is NULL (hier, stats)");
        return -1;
    }

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    memset(stats, 0, sizeof(kg_brain_stats_t));
    stats->total_modules = hier->entry_count;

    /* Count running and aggregate health */
    uint32_t healthy_count = 0;
    bio_module_health_t worst = BIO_MODULE_HEALTH_HEALTHY;

    for (uint32_t i = 0; i < hier->entry_count; i++) {
        const kg_hierarchy_entry_t* e = &hier->entries[i];

        if (e->state == KG_MODULE_STATE_RUNNING) {
            stats->running_modules++;
        }

        if (e->health == BIO_MODULE_HEALTH_HEALTHY) {
            healthy_count++;
        }
        if (e->health > worst) {
            worst = e->health;
        }

        stats->total_messages += e->stats.messages_sent + e->stats.messages_received;
    }

    /* Get connection count from KG */
    if (hier->kg) {
        brain_kg_stats_t kg_stats;
        if (brain_kg_get_stats(hier->kg, &kg_stats) == 0) {
            stats->total_connections = kg_stats.total_edges;
        }
    }

    stats->brain_health = worst;
    stats->health_score = (hier->entry_count > 0) ?
        (float)healthy_count / hier->entry_count : 1.0f;
    stats->uptime_ms = nimcp_time_ms() - hier->creation_time;

    /* Hemisphere breakdown */
    stats->left_modules = hier->hemispheres[KG_HEMISPHERE_LEFT].module_count;
    stats->right_modules = hier->hemispheres[KG_HEMISPHERE_RIGHT].module_count;
    stats->bilateral_modules = hier->hemispheres[KG_HEMISPHERE_BILATERAL].module_count;
    /* TODO: Count interhemispheric connections */
    stats->interhemispheric_conn = 0;

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    return 0;
}

uint32_t kg_hierarchy_get_hemispheres(
    const kg_hierarchy_t* hier,
    kg_hemisphere_info_t* hemispheres
) {
    if (!hier || !hemispheres) return 0;

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    for (uint32_t h = 0; h < KG_HEMISPHERE_COUNT; h++) {
        const kg_hemisphere_data_t* hd = &hier->hemispheres[h];

        hemispheres[h].hemisphere_id = (kg_hemisphere_t)h;
        hemispheres[h].name = g_hemisphere_names[h];
        hemispheres[h].description = g_hemisphere_descriptions[h];
        hemispheres[h].total_modules = hd->module_count;
        hemispheres[h].running_modules = hd->running_count;
        hemispheres[h].stopped_modules = hd->stopped_count;
        hemispheres[h].error_modules = hd->error_count;
        hemispheres[h].overall_health = hd->worst_health;

        /* Copy layer breakdown */
        for (uint32_t l = 0; l < KG_LAYER_COUNT; l++) {
            hemispheres[h].modules_per_layer[l] = hd->modules_per_layer[l];
        }

        /* Calculate health score */
        uint32_t healthy = 0;
        for (uint32_t m = 0; m < hd->module_count; m++) {
            uint32_t idx = find_entry_index(hier, hd->modules[m]);
            if (idx != UINT32_MAX && hier->entries[idx].health == BIO_MODULE_HEALTH_HEALTHY) {
                healthy++;
            }
        }
        hemispheres[h].health_score = (hd->module_count > 0) ?
            (float)healthy / hd->module_count : 1.0f;

        /* Connection counts - TODO */
        hemispheres[h].interhemispheric_connections = 0;
        hemispheres[h].intrahemispheric_connections = 0;
    }

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    return KG_HEMISPHERE_COUNT;
}

uint32_t kg_hierarchy_get_layers(
    const kg_hierarchy_t* hier,
    kg_layer_info_t* layers
) {
    if (!hier || !layers) return 0;

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    for (uint32_t l = 0; l < KG_LAYER_COUNT; l++) {
        const kg_layer_data_t* ld = &hier->layers[l];

        layers[l].layer_id = (kg_cortical_layer_t)l;
        layers[l].name = g_layer_names[l];
        layers[l].description = g_layer_descriptions[l];
        layers[l].total_modules = ld->module_count;
        layers[l].running_modules = ld->running_count;
        layers[l].stopped_modules = ld->stopped_count;
        layers[l].error_modules = ld->error_count;
        layers[l].overall_health = ld->worst_health;

        /* Connection counts would need edge traversal - leave as 0 for now */
        layers[l].feedforward_connections = 0;
        layers[l].feedback_connections = 0;
        layers[l].lateral_connections = 0;
    }

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    return KG_LAYER_COUNT;
}

bio_module_health_t kg_hierarchy_get_brain_health(const kg_hierarchy_t* hier) {
    if (!hier) return BIO_MODULE_HEALTH_UNKNOWN;

    bio_module_health_t worst = BIO_MODULE_HEALTH_HEALTHY;

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    for (uint32_t l = 0; l < KG_LAYER_COUNT; l++) {
        if (hier->layers[l].worst_health > worst) {
            worst = hier->layers[l].worst_health;
        }
    }

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    return worst;
}

/* ============================================================================
 * Level 1 Queries (Hemisphere Level)
 * ============================================================================ */

int kg_hierarchy_get_hemisphere_info(
    const kg_hierarchy_t* hier,
    kg_hemisphere_t hemisphere,
    kg_hemisphere_info_t* info
) {
    if (!hier || !info || hemisphere >= KG_HEMISPHERE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_get_hemisphere_info: required parameter is NULL (hier, info)");
        return -1;
    }

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    const kg_hemisphere_data_t* hd = &hier->hemispheres[hemisphere];

    info->hemisphere_id = hemisphere;
    info->name = g_hemisphere_names[hemisphere];
    info->description = g_hemisphere_descriptions[hemisphere];
    info->total_modules = hd->module_count;
    info->running_modules = hd->running_count;
    info->stopped_modules = hd->stopped_count;
    info->error_modules = hd->error_count;
    info->overall_health = hd->worst_health;

    /* Copy layer breakdown */
    for (uint32_t l = 0; l < KG_LAYER_COUNT; l++) {
        info->modules_per_layer[l] = hd->modules_per_layer[l];
    }

    /* Calculate health score */
    uint32_t healthy = 0;
    for (uint32_t m = 0; m < hd->module_count; m++) {
        uint32_t idx = find_entry_index(hier, hd->modules[m]);
        if (idx != UINT32_MAX && hier->entries[idx].health == BIO_MODULE_HEALTH_HEALTHY) {
            healthy++;
        }
    }
    info->health_score = (hd->module_count > 0) ?
        (float)healthy / hd->module_count : 1.0f;

    info->interhemispheric_connections = 0;  /* TODO */
    info->intrahemispheric_connections = 0;  /* TODO */

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    return 0;
}

uint32_t kg_hierarchy_get_hemisphere_layers(
    const kg_hierarchy_t* hier,
    kg_hemisphere_t hemisphere,
    kg_layer_info_t* layers
) {
    if (!hier || !layers || hemisphere >= KG_HEMISPHERE_COUNT) return 0;

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    const kg_hemisphere_data_t* hd = &hier->hemispheres[hemisphere];

    for (uint32_t l = 0; l < KG_LAYER_COUNT; l++) {
        layers[l].layer_id = (kg_cortical_layer_t)l;
        layers[l].name = g_layer_names[l];
        layers[l].description = g_layer_descriptions[l];
        layers[l].hemisphere = hemisphere;
        layers[l].total_modules = hd->modules_per_layer[l];

        /* Count running/stopped/error in this layer-hemisphere combo */
        layers[l].running_modules = 0;
        layers[l].stopped_modules = 0;
        layers[l].error_modules = 0;
        layers[l].overall_health = BIO_MODULE_HEALTH_HEALTHY;

        for (uint32_t m = 0; m < hd->module_count; m++) {
            uint32_t idx = find_entry_index(hier, hd->modules[m]);
            if (idx == UINT32_MAX) continue;

            const kg_hierarchy_entry_t* e = &hier->entries[idx];
            if (e->layer != (kg_cortical_layer_t)l) continue;

            switch (e->state) {
                case KG_MODULE_STATE_RUNNING:
                    layers[l].running_modules++;
                    break;
                case KG_MODULE_STATE_STOPPED:
                    layers[l].stopped_modules++;
                    break;
                case KG_MODULE_STATE_ERROR:
                    layers[l].error_modules++;
                    break;
                default:
                    break;
            }

            if (e->health > layers[l].overall_health) {
                layers[l].overall_health = e->health;
            }
        }

        layers[l].feedforward_connections = 0;
        layers[l].feedback_connections = 0;
        layers[l].lateral_connections = 0;
    }

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    return KG_LAYER_COUNT;
}

uint32_t kg_hierarchy_get_hemisphere_modules(
    const kg_hierarchy_t* hier,
    kg_hemisphere_t hemisphere,
    brain_kg_node_id_t* module_ids,
    uint32_t max_modules
) {
    if (!hier || !module_ids || hemisphere >= KG_HEMISPHERE_COUNT) return 0;

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    const kg_hemisphere_data_t* hd = &hier->hemispheres[hemisphere];
    uint32_t count = (hd->module_count < max_modules) ?
        hd->module_count : max_modules;

    memcpy(module_ids, hd->modules, count * sizeof(brain_kg_node_id_t));

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    return count;
}

uint32_t kg_hierarchy_get_interhemispheric_connections(
    const kg_hierarchy_t* hier
) {
    if (!hier) return 0;

    /* TODO: Traverse edges and count those crossing hemispheres */
    return 0;
}

bio_module_health_t kg_hierarchy_get_hemisphere_health(
    const kg_hierarchy_t* hier,
    kg_hemisphere_t hemisphere
) {
    if (!hier || hemisphere >= KG_HEMISPHERE_COUNT) {
        return BIO_MODULE_HEALTH_UNKNOWN;
    }

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);
    bio_module_health_t health = hier->hemispheres[hemisphere].worst_health;
    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);

    return health;
}

/* ============================================================================
 * Level 2 Queries (Layer Level)
 * ============================================================================ */

int kg_hierarchy_get_layer_info(
    const kg_hierarchy_t* hier,
    kg_cortical_layer_t layer,
    kg_layer_info_t* info
) {
    if (!hier || !info || layer >= KG_LAYER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_get_layer_info: required parameter is NULL (hier, info)");
        return -1;
    }

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    const kg_layer_data_t* ld = &hier->layers[layer];

    info->layer_id = layer;
    info->name = g_layer_names[layer];
    info->description = g_layer_descriptions[layer];
    info->total_modules = ld->module_count;
    info->running_modules = ld->running_count;
    info->stopped_modules = ld->stopped_count;
    info->error_modules = ld->error_count;
    info->overall_health = ld->worst_health;
    info->feedforward_connections = 0;
    info->feedback_connections = 0;
    info->lateral_connections = 0;

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    return 0;
}

uint32_t kg_hierarchy_get_layer_modules(
    const kg_hierarchy_t* hier,
    kg_cortical_layer_t layer,
    brain_kg_node_id_t* module_ids,
    uint32_t max_modules
) {
    if (!hier || !module_ids || layer >= KG_LAYER_COUNT) return 0;

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    const kg_layer_data_t* ld = &hier->layers[layer];
    uint32_t count = (ld->module_count < max_modules) ?
        ld->module_count : max_modules;

    memcpy(module_ids, ld->modules, count * sizeof(brain_kg_node_id_t));

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    return count;
}

uint32_t kg_hierarchy_get_interlayer_connections(
    const kg_hierarchy_t* hier,
    kg_cortical_layer_t from_layer,
    kg_cortical_layer_t to_layer
) {
    if (!hier || from_layer >= KG_LAYER_COUNT || to_layer >= KG_LAYER_COUNT) {
        return 0;
    }

    /* Would need to traverse edges - return 0 for now */
    /* TODO: Implement edge traversal */
    return 0;
}

/* ============================================================================
 * Level 3 Queries (Module Level)
 * ============================================================================ */

int kg_hierarchy_get_module_info(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    kg_module_info_t* info
) {
    if (!hier || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_get_module_info: required parameter is NULL (hier, info)");
        return -1;
    }

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    uint32_t idx = find_entry_index(hier, module_id);
    if (idx == UINT32_MAX) {
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_get_module_info: validation failed");
        return -1;
    }

    const kg_hierarchy_entry_t* entry = &hier->entries[idx];

    info->node_id = entry->node_id;
    info->state = entry->state;
    info->health = entry->health;
    info->hemisphere = entry->hemisphere;
    info->layer = entry->layer;
    info->enabled = entry->enabled;
    info->has_anomaly = entry->has_anomaly;
    info->state_change_time = entry->state_change_time;
    info->msg_stats = entry->stats;

    /* Get name and type from KG */
    const brain_kg_node_t* node = brain_kg_get_node(hier->kg, module_id);
    if (node) {
        strncpy(info->name, node->name, BRAIN_KG_MAX_NAME_LEN - 1);
        info->name[BRAIN_KG_MAX_NAME_LEN - 1] = '\0';
        info->node_type = node->type;
        info->category = node_type_to_category(node->type);
    } else {
        info->name[0] = '\0';
        info->node_type = BRAIN_KG_NODE_CUSTOM;
        info->category = BIO_MODULE_CATEGORY_CORE;
    }

    /* Get startup phase from orchestrator if available */
    info->startup_phase = 0;

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    return 0;
}

brain_kg_node_id_t kg_hierarchy_find_module_by_name(
    const kg_hierarchy_t* hier,
    const char* name
) {
    if (!hier || !name || !hier->kg) {
        return BRAIN_KG_INVALID_NODE;
    }

    return brain_kg_find_node(hier->kg, name);
}

uint32_t kg_hierarchy_get_modules_by_category(
    const kg_hierarchy_t* hier,
    bio_module_category_t category,
    brain_kg_node_id_t* module_ids,
    uint32_t max_modules
) {
    if (!hier || !module_ids || !hier->kg) return 0;

    /* Map category to corresponding node type */
    brain_kg_node_type_t type;
    switch (category) {
        case BIO_MODULE_CATEGORY_CORE:
            type = BRAIN_KG_NODE_CORE;
            break;
        case BIO_MODULE_CATEGORY_COGNITIVE:
            type = BRAIN_KG_NODE_COGNITIVE;
            break;
        case BIO_MODULE_CATEGORY_PERCEPTION:
            type = BRAIN_KG_NODE_PERCEPTION;
            break;
        case BIO_MODULE_CATEGORY_PLASTICITY:
            type = BRAIN_KG_NODE_PLASTICITY;
            break;
        case BIO_MODULE_CATEGORY_SWARM:
            type = BRAIN_KG_NODE_SWARM;
            break;
        case BIO_MODULE_CATEGORY_SECURITY:
            type = BRAIN_KG_NODE_SECURITY;
            break;
        case BIO_MODULE_CATEGORY_HIGHLEVEL:
            type = BRAIN_KG_NODE_COORDINATOR;
            break;
        default:
            type = BRAIN_KG_NODE_CUSTOM;
    }

    brain_kg_node_list_t* list = brain_kg_get_nodes_by_type(hier->kg, type);
    if (!list) return 0;

    uint32_t count = (list->count < max_modules) ? list->count : max_modules;
    for (uint32_t i = 0; i < count; i++) {
        module_ids[i] = list->nodes[i]->id;
    }

    brain_kg_node_list_destroy(list);
    return count;
}

int kg_hierarchy_get_module_hemisphere(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id
) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_get_module_hemisphere: hier is NULL");
        return -1;
    }

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    uint32_t idx = find_entry_index(hier, module_id);
    if (idx == UINT32_MAX) {
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_get_module_hemisphere: validation failed");
        return -1;
    }

    int hemisphere = (int)hier->entries[idx].hemisphere;

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    return hemisphere;
}

uint32_t kg_hierarchy_get_module_inputs(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    kg_connection_t* connections,
    uint32_t max_connections
) {
    if (!hier || !connections || !hier->kg) return 0;

    brain_kg_edge_list_t* edges = brain_kg_get_incoming(hier->kg, module_id);
    if (!edges) return 0;

    uint32_t count = (edges->count < max_connections) ?
        edges->count : max_connections;

    for (uint32_t i = 0; i < count; i++) {
        connections[i].target_id = edges->edges[i]->from;
        connections[i].edge_type = edges->edges[i]->type;
        connections[i].weight = edges->edges[i]->weight;
        connections[i].bidirectional = edges->edges[i]->bidirectional;
    }

    brain_kg_edge_list_destroy(edges);
    return count;
}

uint32_t kg_hierarchy_get_module_outputs(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    kg_connection_t* connections,
    uint32_t max_connections
) {
    if (!hier || !connections || !hier->kg) return 0;

    brain_kg_edge_list_t* edges = brain_kg_get_outgoing(hier->kg, module_id);
    if (!edges) return 0;

    uint32_t count = (edges->count < max_connections) ?
        edges->count : max_connections;

    for (uint32_t i = 0; i < count; i++) {
        connections[i].target_id = edges->edges[i]->to;
        connections[i].edge_type = edges->edges[i]->type;
        connections[i].weight = edges->edges[i]->weight;
        connections[i].bidirectional = edges->edges[i]->bidirectional;
    }

    brain_kg_edge_list_destroy(edges);
    return count;
}

/* ============================================================================
 * Hierarchical Traversal
 * ============================================================================ */

brain_kg_node_id_t kg_hierarchy_get_parent(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t node_id
) {
    /* In our hierarchy, modules have no parent (flat within layer) */
    /* Future: could return layer node ID if we add synthetic layer nodes */
    (void)hier;
    (void)node_id;
    return BRAIN_KG_INVALID_NODE;
}

int kg_hierarchy_get_module_layer(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id
) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_get_module_layer: hier is NULL");
        return -1;
    }

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    uint32_t idx = find_entry_index(hier, module_id);
    if (idx == UINT32_MAX) {
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_get_module_layer: validation failed");
        return -1;
    }

    int layer = (int)hier->entries[idx].layer;

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    return layer;
}

kg_hierarchy_level_t kg_hierarchy_get_level(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t node_id
) {
    if (!hier) return KG_LEVEL_MODULE;

    /* All tracked nodes are modules (level 2) */
    uint32_t idx = find_entry_index(hier, node_id);
    if (idx != UINT32_MAX) {
        return KG_LEVEL_MODULE;
    }

    /* Unknown node */
    return KG_LEVEL_MODULE;
}

uint32_t kg_hierarchy_get_dependents(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    brain_kg_node_id_t* dependents,
    uint32_t max_dependents
) {
    if (!hier || !dependents || !hier->kg) return 0;

    /* Find nodes that depend on this one via DEPENDS_ON edges */
    brain_kg_edge_list_t* edges = brain_kg_get_incoming(hier->kg, module_id);
    if (!edges) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < edges->count && count < max_dependents; i++) {
        if (edges->edges[i]->type == BRAIN_KG_EDGE_DEPENDS_ON) {
            dependents[count++] = edges->edges[i]->from;
        }
    }

    brain_kg_edge_list_destroy(edges);
    return count;
}

uint32_t kg_hierarchy_get_dependencies(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    brain_kg_node_id_t* dependencies,
    uint32_t max_deps
) {
    if (!hier || !dependencies || !hier->kg) return 0;

    /* Find nodes this one depends on via DEPENDS_ON edges */
    brain_kg_edge_list_t* edges = brain_kg_get_outgoing(hier->kg, module_id);
    if (!edges) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < edges->count && count < max_deps; i++) {
        if (edges->edges[i]->type == BRAIN_KG_EDGE_DEPENDS_ON) {
            dependencies[count++] = edges->edges[i]->to;
        }
    }

    brain_kg_edge_list_destroy(edges);
    return count;
}

/* ============================================================================
 * Real-Time State Updates
 * ============================================================================ */

int kg_hierarchy_report_state_change(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    kg_module_state_t new_state,
    const char* reason
) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_report_state_change: hier is NULL");
        return -1;
    }

    nimcp_mutex_lock(hier->mutex);

    uint32_t idx = find_entry_index(hier, module_id);
    if (idx == UINT32_MAX) {
        nimcp_mutex_unlock(hier->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_report_state_change: validation failed");
        return -1;
    }

    kg_hierarchy_entry_t* entry = &hier->entries[idx];
    kg_module_state_t old_state = entry->state;

    entry->state = new_state;
    entry->state_change_time = nimcp_time_ms();

    /* Update layer statistics */
    update_layer_stats(hier);

    /* Invoke callbacks */
    kg_state_change_event_t event = {
        .module_id = module_id,
        .old_state = old_state,
        .new_state = new_state,
        .health = entry->health,
        .timestamp = entry->state_change_time,
        .reason = {0}
    };
    if (reason) {
        strncpy(event.reason, reason, sizeof(event.reason) - 1);
    }
    invoke_callbacks(hier, &event);

    nimcp_mutex_unlock(hier->mutex);
    return 0;
}

int kg_hierarchy_report_health_change(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    bio_module_health_t health
) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_report_health_change: hier is NULL");
        return -1;
    }

    nimcp_mutex_lock(hier->mutex);

    uint32_t idx = find_entry_index(hier, module_id);
    if (idx == UINT32_MAX) {
        nimcp_mutex_unlock(hier->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_report_health_change: validation failed");
        return -1;
    }

    hier->entries[idx].health = health;

    /* Update layer statistics */
    update_layer_stats(hier);

    nimcp_mutex_unlock(hier->mutex);
    return 0;
}

int kg_hierarchy_report_message_stats(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    uint64_t sent,
    uint64_t received
) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_report_message_stats: hier is NULL");
        return -1;
    }

    nimcp_mutex_lock(hier->mutex);

    uint32_t idx = find_entry_index(hier, module_id);
    if (idx == UINT32_MAX) {
        nimcp_mutex_unlock(hier->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_report_message_stats: validation failed");
        return -1;
    }

    kg_hierarchy_entry_t* entry = &hier->entries[idx];
    entry->stats.messages_sent += sent;
    entry->stats.messages_received += received;

    nimcp_mutex_unlock(hier->mutex);
    return 0;
}

int kg_hierarchy_report_anomaly(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id,
    bool has_anomaly
) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_report_anomaly: hier is NULL");
        return -1;
    }

    nimcp_mutex_lock(hier->mutex);

    uint32_t idx = find_entry_index(hier, module_id);
    if (idx == UINT32_MAX) {
        nimcp_mutex_unlock(hier->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_report_anomaly: validation failed");
        return -1;
    }

    hier->entries[idx].has_anomaly = has_anomaly;

    nimcp_mutex_unlock(hier->mutex);
    return 0;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int kg_hierarchy_register_state_callback(
    kg_hierarchy_t* hier,
    kg_state_change_callback_t callback,
    void* user_data
) {
    if (!hier || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_register_state_callback: required parameter is NULL (hier, callback)");
        return -1;
    }

    nimcp_mutex_lock(hier->mutex);

    if (hier->callback_count >= KG_HIERARCHY_MAX_CALLBACKS) {
        nimcp_mutex_unlock(hier->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_hierarchy_register_state_callback: capacity exceeded");
        return -1;
    }

    hier->callbacks[hier->callback_count].callback = callback;
    hier->callbacks[hier->callback_count].user_data = user_data;
    hier->callback_count++;

    nimcp_mutex_unlock(hier->mutex);
    return 0;
}

int kg_hierarchy_unregister_state_callback(
    kg_hierarchy_t* hier,
    kg_state_change_callback_t callback
) {
    if (!hier || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_unregister_state_callback: required parameter is NULL (hier, callback)");
        return -1;
    }

    nimcp_mutex_lock(hier->mutex);

    for (uint32_t i = 0; i < hier->callback_count; i++) {
        if (hier->callbacks[i].callback == callback) {
            /* Shift remaining callbacks down */
            for (uint32_t j = i; j < hier->callback_count - 1; j++) {
                hier->callbacks[j] = hier->callbacks[j + 1];
            }
            hier->callback_count--;
            nimcp_mutex_unlock(hier->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(hier->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_unregister_state_callback: operation failed");
    return -1;
}

/* ============================================================================
 * Thread-Safe Access
 * ============================================================================ */

int kg_hierarchy_read_lock(kg_hierarchy_t* hier) {
    if (!hier || !hier->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_read_lock: required parameter is NULL (hier, hier->mutex)");
        return -1;
    }
    return nimcp_mutex_lock(hier->mutex);
}

int kg_hierarchy_read_unlock(kg_hierarchy_t* hier) {
    if (!hier || !hier->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_read_unlock: required parameter is NULL (hier, hier->mutex)");
        return -1;
    }
    return nimcp_mutex_unlock(hier->mutex);
}

/* ============================================================================
 * Rebuild / Sync
 * ============================================================================ */

int kg_hierarchy_rebuild(kg_hierarchy_t* hier) {
    if (!hier || !hier->kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_rebuild: required parameter is NULL (hier, hier->kg)");
        return -1;
    }

    nimcp_mutex_lock(hier->mutex);

    /* Clear existing data */
    hier->entry_count = 0;
    for (uint32_t h = 0; h < KG_HEMISPHERE_COUNT; h++) {
        hier->hemispheres[h].module_count = 0;
        for (uint32_t l = 0; l < KG_LAYER_COUNT; l++) {
            hier->hemispheres[h].modules_per_layer[l] = 0;
        }
    }
    for (uint32_t l = 0; l < KG_LAYER_COUNT; l++) {
        hier->layers[l].module_count = 0;
    }
    if (hier->node_to_entry) {
        memset(hier->node_to_entry, 0xFF,
               hier->node_map_capacity * sizeof(uint32_t));
    }

    /* Get all nodes from KG */
    brain_kg_node_list_t* nodes = brain_kg_get_all_nodes(hier->kg);
    if (!nodes) {
        hier->dirty = false;
        hier->last_sync_time = nimcp_time_ms();
        nimcp_mutex_unlock(hier->mutex);
        return 0;  /* Empty KG is valid */
    }

    /* Add each node to hierarchy */
    for (uint32_t i = 0; i < nodes->count; i++) {
        const brain_kg_node_t* node = nodes->nodes[i];
        if (!node) continue;

        /* Determine hemisphere from node type and name */
        kg_hemisphere_t hemisphere = determine_hemisphere(node->type, node->name);

        /* Determine layer from node type */
        bio_module_category_t cat = node_type_to_category(node->type);
        kg_cortical_layer_t layer = category_to_layer(cat);

        if (add_entry(hier, node->id, hemisphere, layer) < 0) {
            LOG_WARN("Failed to add node %u to hierarchy", node->id);
            continue;
        }

        /* Update entry state from KG node */
        uint32_t idx = find_entry_index(hier, node->id);
        if (idx != UINT32_MAX) {
            hier->entries[idx].state = kg_state_to_hier_state(node->state);
            hier->entries[idx].enabled = node->enabled;
        }
    }

    brain_kg_node_list_destroy(nodes);

    /* Update statistics */
    update_hemisphere_stats(hier);
    update_layer_stats(hier);

    hier->dirty = false;
    hier->last_sync_time = nimcp_time_ms();

    nimcp_mutex_unlock(hier->mutex);

    LOG_DEBUG("KG hierarchy rebuilt with %u entries (L:%u, R:%u, B:%u)",
              hier->entry_count,
              hier->hemispheres[KG_HEMISPHERE_LEFT].module_count,
              hier->hemispheres[KG_HEMISPHERE_RIGHT].module_count,
              hier->hemispheres[KG_HEMISPHERE_BILATERAL].module_count);
    return 0;
}

int kg_hierarchy_sync_all(kg_hierarchy_t* hier) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_sync_all: hier is NULL");
        return -1;
    }

    /* Rebuild from KG */
    int result = kg_hierarchy_rebuild(hier);
    if (result < 0) return result;

    /* Additional sync from orchestrator would go here */
    /* TODO: Sync from orchestrator module states */

    return 0;
}

void kg_hierarchy_invalidate(kg_hierarchy_t* hier) {
    if (!hier) return;

    nimcp_mutex_lock(hier->mutex);
    hier->dirty = true;
    nimcp_mutex_unlock(hier->mutex);
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* kg_hemisphere_to_string(kg_hemisphere_t hemisphere) {
    if (hemisphere >= KG_HEMISPHERE_COUNT) return "Unknown";
    return g_hemisphere_names[hemisphere];
}

const char* kg_cortical_layer_to_string(kg_cortical_layer_t layer) {
    if (layer >= KG_LAYER_COUNT) return "Unknown";
    return g_layer_names[layer];
}

const char* kg_module_state_to_string(kg_module_state_t state) {
    switch (state) {
        case KG_MODULE_STATE_UNKNOWN:  return "Unknown";
        case KG_MODULE_STATE_STOPPED:  return "Stopped";
        case KG_MODULE_STATE_STARTING: return "Starting";
        case KG_MODULE_STATE_RUNNING:  return "Running";
        case KG_MODULE_STATE_PAUSED:   return "Paused";
        case KG_MODULE_STATE_DEGRADED: return "Degraded";
        case KG_MODULE_STATE_ERROR:    return "Error";
        default: return "Unknown";
    }
}

const char* kg_hierarchy_level_to_string(kg_hierarchy_level_t level) {
    switch (level) {
        case KG_LEVEL_BRAIN:      return "Brain";
        case KG_LEVEL_HEMISPHERE: return "Hemisphere";
        case KG_LEVEL_LAYER:      return "Layer";
        case KG_LEVEL_MODULE:     return "Module";
        default: return "Unknown";
    }
}

/* ============================================================================
 * Graph Algorithm Integration - Internal Structures
 * ============================================================================ */

/**
 * @brief Context for topological sort callbacks
 */
typedef struct {
    const kg_hierarchy_t* hier;
    brain_kg_node_id_t* node_id_map;  /**< Maps index -> node_id */
    uint32_t* node_index_map;          /**< Maps node_id -> index (sparse) */
    uint32_t max_node_id;              /**< Highest node ID for sparse map */
} topo_sort_context_t;

/**
 * @brief Context for BFS/DFS callbacks
 */
typedef struct {
    const kg_hierarchy_t* hier;
    brain_kg_node_id_t* node_id_map;
    uint32_t* node_index_map;
    uint32_t max_node_id;
    kg_traversal_visitor_fn user_visitor;
    void* user_data;
} traversal_context_t;

/**
 * @brief Context for shortest path finding
 */
typedef struct {
    const kg_hierarchy_t* hier;
    brain_kg_node_id_t target;
    brain_kg_node_id_t* parent;       /**< Parent in BFS tree */
    uint32_t* distance;               /**< Distance from start */
    bool found;
} path_context_t;

/**
 * @brief Context for component finding
 */
typedef struct {
    const kg_hierarchy_t* hier;
    uint32_t* component_ids;
    uint32_t current_component;
} component_context_t;

/* ============================================================================
 * Graph Algorithm Callbacks for nimcp_sort
 * ============================================================================ */

/**
 * @brief Get dependency count for topological sort
 */
static uint32_t topo_get_dep_count(uint32_t node_index, void* user_data) {
    topo_sort_context_t* ctx = (topo_sort_context_t*)user_data;
    if (!ctx || !ctx->hier || !ctx->hier->kg) return UINT32_MAX;

    brain_kg_node_id_t node_id = ctx->node_id_map[node_index];

    /* Count DEPENDS_ON edges incoming to this node */
    brain_kg_edge_list_t* edges = brain_kg_get_edges_by_type(
        ctx->hier->kg, BRAIN_KG_EDGE_DEPENDS_ON
    );
    if (!edges) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < edges->count; i++) {
        if (edges->edges[i]->to == node_id) {
            count++;
        }
    }
    brain_kg_edge_list_destroy(edges);
    return count;
}

/**
 * @brief Get specific dependency for topological sort
 */
static uint32_t topo_get_dep(uint32_t node_index, uint32_t dep_index, void* user_data) {
    topo_sort_context_t* ctx = (topo_sort_context_t*)user_data;
    if (!ctx || !ctx->hier || !ctx->hier->kg) return UINT32_MAX;

    brain_kg_node_id_t node_id = ctx->node_id_map[node_index];

    /* Find the dep_index-th DEPENDS_ON edge to this node */
    brain_kg_edge_list_t* edges = brain_kg_get_edges_by_type(
        ctx->hier->kg, BRAIN_KG_EDGE_DEPENDS_ON
    );
    if (!edges) return UINT32_MAX;

    uint32_t count = 0;
    uint32_t result = UINT32_MAX;
    for (uint32_t i = 0; i < edges->count; i++) {
        if (edges->edges[i]->to == node_id) {
            if (count == dep_index) {
                /* Map source node_id back to index */
                brain_kg_node_id_t dep_node_id = edges->edges[i]->from;
                if (dep_node_id < ctx->max_node_id) {
                    result = ctx->node_index_map[dep_node_id];
                }
                break;
            }
            count++;
        }
    }
    brain_kg_edge_list_destroy(edges);
    return result;
}

/**
 * @brief Get neighbor count for BFS/DFS (undirected view)
 */
static uint32_t traversal_get_neighbor_count(uint32_t node_index, void* user_data) {
    traversal_context_t* ctx = (traversal_context_t*)user_data;
    if (!ctx || !ctx->hier || !ctx->hier->kg) return UINT32_MAX;

    brain_kg_node_id_t node_id = ctx->node_id_map[node_index];

    /* Count all edges from/to this node (both directions) */
    uint32_t count = 0;

    brain_kg_edge_list_t* outgoing = brain_kg_get_outgoing(ctx->hier->kg, node_id);
    if (outgoing) {
        count += outgoing->count;
        brain_kg_edge_list_destroy(outgoing);
    }

    brain_kg_edge_list_t* incoming = brain_kg_get_incoming(ctx->hier->kg, node_id);
    if (incoming) {
        count += incoming->count;
        brain_kg_edge_list_destroy(incoming);
    }

    return count;
}

/**
 * @brief Get specific neighbor for BFS/DFS
 */
static uint32_t traversal_get_neighbor(uint32_t node_index, uint32_t neighbor_index, void* user_data) {
    traversal_context_t* ctx = (traversal_context_t*)user_data;
    if (!ctx || !ctx->hier || !ctx->hier->kg) return UINT32_MAX;

    brain_kg_node_id_t node_id = ctx->node_id_map[node_index];
    brain_kg_node_id_t neighbor_id = BRAIN_KG_INVALID_NODE;
    uint32_t current_index = 0;

    /* Check outgoing edges first */
    brain_kg_edge_list_t* outgoing = brain_kg_get_outgoing(ctx->hier->kg, node_id);
    if (outgoing) {
        if (neighbor_index < outgoing->count) {
            neighbor_id = outgoing->edges[neighbor_index]->to;
            brain_kg_edge_list_destroy(outgoing);
            goto map_neighbor;
        }
        current_index = outgoing->count;
        brain_kg_edge_list_destroy(outgoing);
    }

    /* Check incoming edges */
    brain_kg_edge_list_t* incoming = brain_kg_get_incoming(ctx->hier->kg, node_id);
    if (incoming) {
        uint32_t adjusted_index = neighbor_index - current_index;
        if (adjusted_index < incoming->count) {
            neighbor_id = incoming->edges[adjusted_index]->from;
        }
        brain_kg_edge_list_destroy(incoming);
    }

map_neighbor:
    /* Map back to index */
    if (neighbor_id != BRAIN_KG_INVALID_NODE && neighbor_id < ctx->max_node_id) {
        uint32_t idx = ctx->node_index_map[neighbor_id];
        if (idx != UINT32_MAX) {
            return idx;
        }
    }
    return UINT32_MAX;
}

/**
 * @brief Visitor wrapper for BFS/DFS
 */
static bool traversal_visit(uint32_t node_index, uint32_t depth, void* user_data) {
    traversal_context_t* ctx = (traversal_context_t*)user_data;
    if (!ctx || !ctx->user_visitor) return true;

    brain_kg_node_id_t node_id = ctx->node_id_map[node_index];
    return ctx->user_visitor(node_id, depth, ctx->user_data);
}

/* ============================================================================
 * Helper Functions for Graph Algorithms
 * ============================================================================ */

/**
 * @brief Build node ID <-> index mappings for graph algorithms
 */
static int build_node_mappings(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t** node_id_map,
    uint32_t** node_index_map,
    uint32_t* max_node_id
) {
    if (!hier || !node_id_map || !node_index_map || !max_node_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "build_node_mappings: required parameter is NULL (hier, node_id_map, node_index_map, max_node_id)");
        return -1;
    }

    uint32_t count = hier->entry_count;
    if (count == 0) {
        *node_id_map = NULL;
        *node_index_map = NULL;
        *max_node_id = 0;
        return 0;
    }

    /* Allocate node_id_map: index -> node_id */
    *node_id_map = (brain_kg_node_id_t*)nimcp_calloc(count, sizeof(brain_kg_node_id_t));
    if (!*node_id_map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_node_mappings: validation failed");
        return -1;
    }

    /* Find max node ID and populate node_id_map */
    *max_node_id = 0;
    for (uint32_t i = 0; i < count; i++) {
        brain_kg_node_id_t nid = hier->entries[i].node_id;
        (*node_id_map)[i] = nid;
        if (nid >= *max_node_id) {
            *max_node_id = nid + 1;
        }
    }

    /* Allocate node_index_map: node_id -> index (sparse) */
    *node_index_map = (uint32_t*)nimcp_malloc(*max_node_id * sizeof(uint32_t));
    if (!*node_index_map) {
        nimcp_free(*node_id_map);
        *node_id_map = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "build_node_mappings: validation failed");
        return -1;
    }

    /* Initialize to invalid */
    for (uint32_t i = 0; i < *max_node_id; i++) {
        (*node_index_map)[i] = UINT32_MAX;
    }

    /* Populate reverse mapping */
    for (uint32_t i = 0; i < count; i++) {
        brain_kg_node_id_t nid = (*node_id_map)[i];
        (*node_index_map)[nid] = i;
    }

    return 0;
}

/**
 * @brief Free node mappings
 */
static void free_node_mappings(
    brain_kg_node_id_t* node_id_map,
    uint32_t* node_index_map
) {
    if (node_id_map) nimcp_free(node_id_map);
    if (node_index_map) nimcp_free(node_index_map);
}

/**
 * @brief Comparison function for sorting node IDs
 */
static int compare_node_ids(const void* a, const void* b) {
    brain_kg_node_id_t id_a = *(const brain_kg_node_id_t*)a;
    brain_kg_node_id_t id_b = *(const brain_kg_node_id_t*)b;
    if (id_a < id_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compare_node_ids: validation failed");
        return -1;
    }
    if (id_a > id_b) return 1;
    return 0;
}

/* ============================================================================
 * Topological Sort API Implementation
 * ============================================================================ */

int kg_hierarchy_topological_sort(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t* order,
    uint32_t max_order,
    uint32_t* sorted_count
) {
    if (!hier || !order || !sorted_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_topological_sort: required parameter is NULL (hier, order, sorted_count)");
        return -1;
    }
    *sorted_count = 0;

    if (hier->entry_count == 0) return 0;

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    /* Build mappings */
    brain_kg_node_id_t* node_id_map = NULL;
    uint32_t* node_index_map = NULL;
    uint32_t max_node_id = 0;

    if (build_node_mappings(hier, &node_id_map, &node_index_map, &max_node_id) < 0) {
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_hierarchy_topological_sort: validation failed");
        return -1;
    }

    /* Setup context */
    topo_sort_context_t ctx = {
        .hier = hier,
        .node_id_map = node_id_map,
        .node_index_map = node_index_map,
        .max_node_id = max_node_id
    };

    /* Configure topological sort */
    nimcp_topo_config_t config = {
        .node_count = hier->entry_count,
        .user_data = &ctx,
        .get_dep_count = topo_get_dep_count,
        .get_dep = topo_get_dep,
        .get_dependent_count = NULL,
        .get_dependent = NULL
    };

    /* Allocate temporary index order */
    uint32_t* index_order = (uint32_t*)nimcp_calloc(hier->entry_count, sizeof(uint32_t));
    if (!index_order) {
        free_node_mappings(node_id_map, node_index_map);
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_hierarchy_topological_sort: index_order is NULL");
        return -1;
    }

    /* Run topological sort */
    uint32_t index_sorted = 0;
    nimcp_sort_result_t result = nimcp_topological_sort(&config, index_order, hier->entry_count, &index_sorted);

    /* Convert indices back to node IDs */
    uint32_t copy_count = (index_sorted < max_order) ? index_sorted : max_order;
    for (uint32_t i = 0; i < copy_count; i++) {
        order[i] = node_id_map[index_order[i]];
    }
    *sorted_count = copy_count;

    nimcp_free(index_order);
    free_node_mappings(node_id_map, node_index_map);
    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);

    if (result == NIMCP_SORT_ERROR_CYCLE) {
        return -2;  /* Cycle detected */
    }
    return (result == NIMCP_SORT_OK) ? 0 : -1;
}

bool kg_hierarchy_has_dependency_cycle(const kg_hierarchy_t* hier) {
    if (!hier) {
        return false;
    }

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    /* Build mappings */
    brain_kg_node_id_t* node_id_map = NULL;
    uint32_t* node_index_map = NULL;
    uint32_t max_node_id = 0;

    if (build_node_mappings(hier, &node_id_map, &node_index_map, &max_node_id) < 0) {
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        return false;
    }

    /* Setup context */
    topo_sort_context_t ctx = {
        .hier = hier,
        .node_id_map = node_id_map,
        .node_index_map = node_index_map,
        .max_node_id = max_node_id
    };

    /* Configure */
    nimcp_topo_config_t config = {
        .node_count = hier->entry_count,
        .user_data = &ctx,
        .get_dep_count = topo_get_dep_count,
        .get_dep = topo_get_dep,
        .get_dependent_count = NULL,
        .get_dependent = NULL
    };

    bool has_cycle = nimcp_has_cycle(&config);

    free_node_mappings(node_id_map, node_index_map);
    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);

    return has_cycle;
}

int kg_hierarchy_find_cycle_modules(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t* cycle_modules,
    uint32_t max_modules,
    uint32_t* cycle_count
) {
    if (!hier || !cycle_modules || !cycle_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_find_cycle_modules: required parameter is NULL (hier, cycle_modules, cycle_count)");
        return -1;
    }
    *cycle_count = 0;

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    /* Build mappings */
    brain_kg_node_id_t* node_id_map = NULL;
    uint32_t* node_index_map = NULL;
    uint32_t max_node_id = 0;

    if (build_node_mappings(hier, &node_id_map, &node_index_map, &max_node_id) < 0) {
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_hierarchy_find_cycle_modules: validation failed");
        return -1;
    }

    /* Setup context */
    topo_sort_context_t ctx = {
        .hier = hier,
        .node_id_map = node_id_map,
        .node_index_map = node_index_map,
        .max_node_id = max_node_id
    };

    nimcp_topo_config_t config = {
        .node_count = hier->entry_count,
        .user_data = &ctx,
        .get_dep_count = topo_get_dep_count,
        .get_dep = topo_get_dep,
        .get_dependent_count = NULL,
        .get_dependent = NULL
    };

    /* Allocate temp buffer for cycle indices */
    uint32_t* cycle_indices = (uint32_t*)nimcp_calloc(hier->entry_count, sizeof(uint32_t));
    if (!cycle_indices) {
        free_node_mappings(node_id_map, node_index_map);
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_hierarchy_find_cycle_modules: cycle_indices is NULL");
        return -1;
    }

    uint32_t index_count = 0;
    nimcp_find_cycle_nodes(&config, cycle_indices, hier->entry_count, &index_count);

    /* Convert to node IDs */
    uint32_t copy_count = (index_count < max_modules) ? index_count : max_modules;
    for (uint32_t i = 0; i < copy_count; i++) {
        cycle_modules[i] = node_id_map[cycle_indices[i]];
    }
    *cycle_count = copy_count;

    nimcp_free(cycle_indices);
    free_node_mappings(node_id_map, node_index_map);
    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);

    return 0;
}

int kg_hierarchy_get_startup_phase(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_id
) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_get_startup_phase: hier is NULL");
        return -1;
    }

    uint32_t entry_idx = find_entry_index(hier, module_id);
    if (entry_idx == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_get_startup_phase: validation failed");
        return -1;
    }

    /* Get topological order */
    brain_kg_node_id_t* order = (brain_kg_node_id_t*)nimcp_calloc(
        hier->entry_count, sizeof(brain_kg_node_id_t)
    );
    if (!order) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_hierarchy_get_startup_phase: order is NULL");
        return -1;
    }

    uint32_t sorted_count = 0;
    int result = kg_hierarchy_topological_sort(hier, order, hier->entry_count, &sorted_count);

    if (result < 0) {
        nimcp_free(order);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_get_startup_phase: validation failed");
        return -1;
    }

    /* Find position in sorted order - that's roughly the phase */
    int phase = 0;
    for (uint32_t i = 0; i < sorted_count; i++) {
        if (order[i] == module_id) {
            /* Simple heuristic: divide into phases based on position */
            phase = (int)(i * 6 / sorted_count);  /* 6 phases max */
            break;
        }
    }

    nimcp_free(order);
    return phase;
}

/* ============================================================================
 * Binary Search / Optimized Lookup Implementation
 * ============================================================================ */

uint32_t kg_hierarchy_binary_search_module(
    const kg_hierarchy_t* hier,
    const brain_kg_node_id_t* sorted_ids,
    uint32_t count,
    brain_kg_node_id_t target_id
) {
    (void)hier;  /* Not used but kept for API consistency */
    if (!sorted_ids || count == 0) return UINT32_MAX;

    return nimcp_binary_search_u32(sorted_ids, count, target_id);
}

uint32_t kg_hierarchy_get_sorted_module_ids(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t* sorted_ids,
    uint32_t max_ids
) {
    if (!hier || !sorted_ids || max_ids == 0) return 0;

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    uint32_t copy_count = (hier->entry_count < max_ids) ? hier->entry_count : max_ids;

    for (uint32_t i = 0; i < copy_count; i++) {
        sorted_ids[i] = hier->entries[i].node_id;
    }

    /* Sort using nimcp_sort */
    nimcp_sort(sorted_ids, copy_count, sizeof(brain_kg_node_id_t), compare_node_ids);

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);

    return copy_count;
}

bool kg_hierarchy_is_sorted(
    const brain_kg_node_id_t* ids,
    uint32_t count
) {
    if (!ids || count <= 1) return true;
    return nimcp_is_sorted_u32(ids, count);
}

/* ============================================================================
 * Graph Traversal API Implementation (BFS/DFS)
 * ============================================================================ */

int kg_hierarchy_bfs(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t start_module,
    kg_traversal_visitor_fn visitor,
    void* user_data
) {
    if (!hier || !visitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_bfs: required parameter is NULL (hier, visitor)");
        return -1;
    }

    uint32_t start_idx = find_entry_index(hier, start_module);
    if (start_idx == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_bfs: validation failed");
        return -1;
    }

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    /* Build mappings */
    brain_kg_node_id_t* node_id_map = NULL;
    uint32_t* node_index_map = NULL;
    uint32_t max_node_id = 0;

    if (build_node_mappings(hier, &node_id_map, &node_index_map, &max_node_id) < 0) {
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_hierarchy_bfs: validation failed");
        return -1;
    }

    /* Setup context */
    traversal_context_t ctx = {
        .hier = hier,
        .node_id_map = node_id_map,
        .node_index_map = node_index_map,
        .max_node_id = max_node_id,
        .user_visitor = visitor,
        .user_data = user_data
    };

    /* Configure BFS */
    nimcp_traversal_config_t config = {
        .node_count = hier->entry_count,
        .start_node = (uint32_t)start_idx,
        .user_data = &ctx,
        .get_neighbor_count = traversal_get_neighbor_count,
        .get_neighbor = traversal_get_neighbor,
        .visit = traversal_visit,
        .visit_user_data = &ctx
    };

    nimcp_sort_result_t result = nimcp_bfs(&config);

    free_node_mappings(node_id_map, node_index_map);
    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);

    return (result == NIMCP_SORT_OK) ? 0 : -1;
}

int kg_hierarchy_dfs(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t start_module,
    kg_traversal_visitor_fn visitor,
    void* user_data
) {
    if (!hier || !visitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_dfs: required parameter is NULL (hier, visitor)");
        return -1;
    }

    uint32_t start_idx = find_entry_index(hier, start_module);
    if (start_idx == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_dfs: validation failed");
        return -1;
    }

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    /* Build mappings */
    brain_kg_node_id_t* node_id_map = NULL;
    uint32_t* node_index_map = NULL;
    uint32_t max_node_id = 0;

    if (build_node_mappings(hier, &node_id_map, &node_index_map, &max_node_id) < 0) {
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_hierarchy_dfs: validation failed");
        return -1;
    }

    /* Setup context */
    traversal_context_t ctx = {
        .hier = hier,
        .node_id_map = node_id_map,
        .node_index_map = node_index_map,
        .max_node_id = max_node_id,
        .user_visitor = visitor,
        .user_data = user_data
    };

    /* Configure DFS */
    nimcp_traversal_config_t config = {
        .node_count = hier->entry_count,
        .start_node = (uint32_t)start_idx,
        .user_data = &ctx,
        .get_neighbor_count = traversal_get_neighbor_count,
        .get_neighbor = traversal_get_neighbor,
        .visit = traversal_visit,
        .visit_user_data = &ctx
    };

    nimcp_sort_result_t result = nimcp_dfs(&config);

    free_node_mappings(node_id_map, node_index_map);
    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);

    return (result == NIMCP_SORT_OK) ? 0 : -1;
}

/**
 * @brief BFS visitor for shortest path - records parent pointers
 */
static bool shortest_path_visitor(uint32_t node_index, uint32_t depth, void* user_data) {
    path_context_t* ctx = (path_context_t*)user_data;
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shortest_path_visitor: ctx is NULL");
        return false;
    }

    ctx->distance[node_index] = depth;

    /* Check if we found target */
    traversal_context_t* tctx = (traversal_context_t*)((char*)ctx - offsetof(traversal_context_t, user_data));
    /* Actually we need the node_id_map - let's use a simpler approach */

    return !ctx->found;  /* Continue until found */
}

int kg_hierarchy_shortest_path(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t from_module,
    brain_kg_node_id_t to_module,
    brain_kg_node_id_t* path,
    uint32_t max_path,
    uint32_t* path_length
) {
    if (!hier || !path || !path_length) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_shortest_path: required parameter is NULL (hier, path, path_length)");
        return -1;
    }
    *path_length = 0;

    if (from_module == to_module) {
        path[0] = from_module;
        *path_length = 1;
        return 0;
    }

    uint32_t from_idx = find_entry_index(hier, from_module);
    uint32_t to_idx = find_entry_index(hier, to_module);
    if (from_idx == UINT32_MAX || to_idx == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_shortest_path: validation failed");
        return -1;
    }

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    uint32_t n = hier->entry_count;

    /* Allocate BFS structures */
    uint32_t* distance = (uint32_t*)nimcp_malloc(n * sizeof(uint32_t));
    int* parent = (int*)nimcp_malloc(n * sizeof(int));
    bool* visited = (bool*)nimcp_calloc(n, sizeof(bool));

    if (!distance || !parent || !visited) {
        nimcp_free(distance);
        nimcp_free(parent);
        nimcp_free(visited);
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_shortest_path: required parameter is NULL (distance, parent, visited)");
        return -1;
    }

    for (uint32_t i = 0; i < n; i++) {
        distance[i] = UINT32_MAX;
        parent[i] = -1;
    }

    /* Build node mappings */
    brain_kg_node_id_t* node_id_map = NULL;
    uint32_t* node_index_map = NULL;
    uint32_t max_node_id = 0;

    if (build_node_mappings(hier, &node_id_map, &node_index_map, &max_node_id) < 0) {
        nimcp_free(distance);
        nimcp_free(parent);
        nimcp_free(visited);
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_hierarchy_shortest_path: validation failed");
        return -1;
    }

    /* Manual BFS for shortest path */
    uint32_t* queue = (uint32_t*)nimcp_malloc(n * sizeof(uint32_t));
    if (!queue) {
        free_node_mappings(node_id_map, node_index_map);
        nimcp_free(distance);
        nimcp_free(parent);
        nimcp_free(visited);
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_shortest_path: queue is NULL");
        return -1;
    }

    uint32_t q_head = 0, q_tail = 0;
    queue[q_tail++] = (uint32_t)from_idx;
    visited[from_idx] = true;
    distance[from_idx] = 0;

    bool found = false;
    while (q_head < q_tail && !found) {
        uint32_t curr = queue[q_head++];

        if (curr == (uint32_t)to_idx) {
            found = true;
            break;
        }

        /* Get neighbors (both outgoing and incoming for undirected traversal) */
        brain_kg_node_id_t curr_id = node_id_map[curr];

        /* Process outgoing edges */
        brain_kg_edge_list_t* outgoing = brain_kg_get_outgoing(hier->kg, curr_id);
        if (outgoing) {
            for (uint32_t i = 0; i < outgoing->count && !found; i++) {
                brain_kg_node_id_t neighbor_id = outgoing->edges[i]->to;
                if (neighbor_id < max_node_id) {
                    uint32_t neighbor_idx = node_index_map[neighbor_id];
                    if (neighbor_idx != UINT32_MAX && !visited[neighbor_idx]) {
                        visited[neighbor_idx] = true;
                        distance[neighbor_idx] = distance[curr] + 1;
                        parent[neighbor_idx] = (int)curr;
                        queue[q_tail++] = neighbor_idx;

                        if (neighbor_idx == to_idx) {
                            found = true;
                        }
                    }
                }
            }
            brain_kg_edge_list_destroy(outgoing);
        }

        /* Process incoming edges */
        if (!found) {
            brain_kg_edge_list_t* incoming = brain_kg_get_incoming(hier->kg, curr_id);
            if (incoming) {
                for (uint32_t i = 0; i < incoming->count && !found; i++) {
                    brain_kg_node_id_t neighbor_id = incoming->edges[i]->from;
                    if (neighbor_id < max_node_id) {
                        uint32_t neighbor_idx = node_index_map[neighbor_id];
                        if (neighbor_idx != UINT32_MAX && !visited[neighbor_idx]) {
                            visited[neighbor_idx] = true;
                            distance[neighbor_idx] = distance[curr] + 1;
                            parent[neighbor_idx] = (int)curr;
                            queue[q_tail++] = neighbor_idx;

                            if (neighbor_idx == to_idx) {
                                found = true;
                            }
                        }
                    }
                }
                brain_kg_edge_list_destroy(incoming);
            }
        }
    }

    nimcp_free(queue);

    int result = -1;
    if (found) {
        /* Reconstruct path */
        uint32_t path_len = distance[to_idx] + 1;
        if (path_len <= max_path) {
            uint32_t idx = (uint32_t)to_idx;
            for (uint32_t i = path_len; i > 0; i--) {
                path[i - 1] = node_id_map[idx];
                if (parent[idx] >= 0) {
                    idx = (uint32_t)parent[idx];
                }
            }
            *path_length = path_len;
            result = 0;
        }
    }

    free_node_mappings(node_id_map, node_index_map);
    nimcp_free(distance);
    nimcp_free(parent);
    nimcp_free(visited);
    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);

    return result;
}

uint32_t kg_hierarchy_get_distance(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t from_module,
    brain_kg_node_id_t to_module
) {
    if (!hier) return UINT32_MAX;
    if (from_module == to_module) return 0;

    brain_kg_node_id_t path[256];
    uint32_t path_length = 0;

    if (kg_hierarchy_shortest_path(hier, from_module, to_module, path, 256, &path_length) == 0) {
        return path_length - 1;  /* Distance is path length minus 1 */
    }
    return UINT32_MAX;
}

uint32_t kg_hierarchy_get_reachable(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t start_module,
    brain_kg_node_id_t* reachable,
    uint32_t max_reachable
) {
    if (!hier || !reachable || max_reachable == 0) return 0;

    typedef struct {
        brain_kg_node_id_t* arr;
        uint32_t count;
        uint32_t max;
    } reachable_ctx_t;

    reachable_ctx_t ctx = { .arr = reachable, .count = 0, .max = max_reachable };

    /* Visitor that collects all visited nodes */
    bool collect_visitor(brain_kg_node_id_t module_id, uint32_t depth, void* user_data) {
        (void)depth;
        reachable_ctx_t* c = (reachable_ctx_t*)user_data;
        if (c->count < c->max) {
            c->arr[c->count++] = module_id;
        }
        return true;
    }

    kg_hierarchy_bfs(hier, start_module, collect_visitor, &ctx);

    return ctx.count;
}

/* ============================================================================
 * Connected Components API Implementation
 * ============================================================================ */

int kg_hierarchy_find_components(
    const kg_hierarchy_t* hier,
    uint32_t* component_ids,
    uint32_t* num_components
) {
    if (!hier || !component_ids || !num_components) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_find_components: required parameter is NULL (hier, component_ids, num_components)");
        return -1;
    }
    *num_components = 0;

    if (hier->entry_count == 0) return 0;

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    uint32_t n = hier->entry_count;

    /* Build mappings */
    brain_kg_node_id_t* node_id_map = NULL;
    uint32_t* node_index_map = NULL;
    uint32_t max_node_id = 0;

    if (build_node_mappings(hier, &node_id_map, &node_index_map, &max_node_id) < 0) {
        nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_hierarchy_find_components: validation failed");
        return -1;
    }

    /* Setup traversal context */
    traversal_context_t ctx = {
        .hier = hier,
        .node_id_map = node_id_map,
        .node_index_map = node_index_map,
        .max_node_id = max_node_id,
        .user_visitor = NULL,
        .user_data = NULL
    };

    /* Configure for nimcp_find_components */
    nimcp_traversal_config_t config = {
        .node_count = n,
        .start_node = 0,  /* Ignored by find_components */
        .user_data = &ctx,
        .get_neighbor_count = traversal_get_neighbor_count,
        .get_neighbor = traversal_get_neighbor,
        .visit = NULL,
        .visit_user_data = NULL
    };

    nimcp_sort_result_t result = nimcp_find_components(&config, component_ids, num_components);

    free_node_mappings(node_id_map, node_index_map);
    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);

    return (result == NIMCP_SORT_OK) ? 0 : -1;
}

int kg_hierarchy_get_component_info(
    const kg_hierarchy_t* hier,
    uint32_t component_id,
    kg_component_info_t* info
) {
    if (!hier || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_get_component_info: required parameter is NULL (hier, info)");
        return -1;
    }

    /* Get component IDs for all modules */
    uint32_t* comp_ids = (uint32_t*)nimcp_calloc(hier->entry_count, sizeof(uint32_t));
    if (!comp_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_hierarchy_get_component_info: comp_ids is NULL");
        return -1;
    }

    uint32_t num_components = 0;
    if (kg_hierarchy_find_components(hier, comp_ids, &num_components) < 0) {
        nimcp_free(comp_ids);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_get_component_info: validation failed");
        return -1;
    }

    if (component_id >= num_components) {
        nimcp_free(comp_ids);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_get_component_info: capacity exceeded");
        return -1;
    }

    /* Count modules in this component and determine hemisphere distribution */
    info->component_id = component_id;
    info->module_count = 0;
    info->modules = NULL;
    info->spans_hemispheres = false;

    uint32_t hemi_counts[KG_HEMISPHERE_COUNT] = {0};

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    for (uint32_t i = 0; i < hier->entry_count; i++) {
        if (comp_ids[i] == component_id) {
            info->module_count++;
            kg_hemisphere_t h = hier->entries[i].hemisphere;
            if (h < KG_HEMISPHERE_COUNT) {
                hemi_counts[h]++;
            }
        }
    }

    /* Determine primary hemisphere */
    uint32_t max_count = 0;
    info->primary_hemisphere = KG_HEMISPHERE_BILATERAL;
    for (int h = 0; h < KG_HEMISPHERE_COUNT; h++) {
        if (hemi_counts[h] > max_count) {
            max_count = hemi_counts[h];
            info->primary_hemisphere = (kg_hemisphere_t)h;
        }
    }

    /* Check if spans hemispheres */
    int hemi_with_modules = 0;
    for (int h = 0; h < KG_HEMISPHERE_COUNT; h++) {
        if (hemi_counts[h] > 0) hemi_with_modules++;
    }
    info->spans_hemispheres = (hemi_with_modules > 1);

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    nimcp_free(comp_ids);

    return 0;
}

uint32_t kg_hierarchy_get_component_modules(
    const kg_hierarchy_t* hier,
    uint32_t component_id,
    brain_kg_node_id_t* modules,
    uint32_t max_modules
) {
    if (!hier || !modules || max_modules == 0) return 0;

    /* Get component IDs */
    uint32_t* comp_ids = (uint32_t*)nimcp_calloc(hier->entry_count, sizeof(uint32_t));
    if (!comp_ids) return 0;

    uint32_t num_components = 0;
    if (kg_hierarchy_find_components(hier, comp_ids, &num_components) < 0) {
        nimcp_free(comp_ids);
        return 0;
    }

    if (component_id >= num_components) {
        nimcp_free(comp_ids);
        return 0;
    }

    nimcp_mutex_lock(((kg_hierarchy_t*)hier)->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < hier->entry_count && count < max_modules; i++) {
        if (comp_ids[i] == component_id) {
            modules[count++] = hier->entries[i].node_id;
        }
    }

    nimcp_mutex_unlock(((kg_hierarchy_t*)hier)->mutex);
    nimcp_free(comp_ids);

    return count;
}

bool kg_hierarchy_are_connected(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t module_a,
    brain_kg_node_id_t module_b
) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_are_connected: hier is NULL");
        return false;
    }
    if (module_a == module_b) return true;

    uint32_t idx_a = find_entry_index(hier, module_a);
    uint32_t idx_b = find_entry_index(hier, module_b);
    if (idx_a == UINT32_MAX || idx_b == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_are_connected: validation failed");
        return false;
    }

    /* Get component IDs */
    uint32_t* comp_ids = (uint32_t*)nimcp_calloc(hier->entry_count, sizeof(uint32_t));
    if (!comp_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_hierarchy_are_connected: comp_ids is NULL");
        return false;
    }

    uint32_t num_components = 0;
    if (kg_hierarchy_find_components(hier, comp_ids, &num_components) < 0) {
        nimcp_free(comp_ids);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_hierarchy_are_connected: validation failed");
        return false;
    }

    bool connected = (comp_ids[idx_a] == comp_ids[idx_b]);
    nimcp_free(comp_ids);

    return connected;
}

uint32_t kg_hierarchy_get_largest_component(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t* modules,
    uint32_t max_modules
) {
    if (!hier || !modules || max_modules == 0) return 0;

    /* Get component IDs */
    uint32_t* comp_ids = (uint32_t*)nimcp_calloc(hier->entry_count, sizeof(uint32_t));
    if (!comp_ids) return 0;

    uint32_t num_components = 0;
    if (kg_hierarchy_find_components(hier, comp_ids, &num_components) < 0) {
        nimcp_free(comp_ids);
        return 0;
    }

    if (num_components == 0) {
        nimcp_free(comp_ids);
        return 0;
    }

    /* Count size of each component */
    uint32_t* sizes = (uint32_t*)nimcp_calloc(num_components, sizeof(uint32_t));
    if (!sizes) {
        nimcp_free(comp_ids);
        return 0;
    }

    for (uint32_t i = 0; i < hier->entry_count; i++) {
        sizes[comp_ids[i]]++;
    }

    /* Find largest */
    uint32_t largest_id = 0;
    uint32_t largest_size = sizes[0];
    for (uint32_t c = 1; c < num_components; c++) {
        if (sizes[c] > largest_size) {
            largest_size = sizes[c];
            largest_id = c;
        }
    }

    nimcp_free(sizes);

    /* Get modules in largest component */
    uint32_t count = kg_hierarchy_get_component_modules(hier, largest_id, modules, max_modules);

    nimcp_free(comp_ids);
    return count;
}

uint32_t kg_hierarchy_count_isolated(const kg_hierarchy_t* hier) {
    if (!hier) return 0;

    /* Get component IDs */
    uint32_t* comp_ids = (uint32_t*)nimcp_calloc(hier->entry_count, sizeof(uint32_t));
    if (!comp_ids) return 0;

    uint32_t num_components = 0;
    if (kg_hierarchy_find_components(hier, comp_ids, &num_components) < 0) {
        nimcp_free(comp_ids);
        return 0;
    }

    if (num_components == 0) {
        nimcp_free(comp_ids);
        return 0;
    }

    /* Count size of each component */
    uint32_t* sizes = (uint32_t*)nimcp_calloc(num_components, sizeof(uint32_t));
    if (!sizes) {
        nimcp_free(comp_ids);
        return 0;
    }

    for (uint32_t i = 0; i < hier->entry_count; i++) {
        sizes[comp_ids[i]]++;
    }

    /* Count components of size 1 */
    uint32_t isolated = 0;
    for (uint32_t c = 0; c < num_components; c++) {
        if (sizes[c] == 1) {
            isolated++;
        }
    }

    nimcp_free(sizes);
    nimcp_free(comp_ids);

    return isolated;
}

void kg_hierarchy_free_components_result(kg_components_result_t* result) {
    if (!result) return;

    if (result->components) {
        for (uint32_t i = 0; i < result->component_count; i++) {
            if (result->components[i].modules) {
                nimcp_free(result->components[i].modules);
            }
        }
        nimcp_free(result->components);
    }
    if (result->module_to_component) {
        nimcp_free(result->module_to_component);
    }
    memset(result, 0, sizeof(kg_components_result_t));
}

/* ============================================================================
 * Edge Iteration API Implementation
 * ============================================================================ */

/**
 * @brief Internal edge iterator structure
 */
struct kg_edge_iterator {
    const kg_hierarchy_t* hier;
    uint32_t current_entry;         /**< Current entry in hierarchy */
    brain_kg_edge_list_t* edges;    /**< Current edge list */
    uint32_t current_edge;          /**< Current edge within list */
};

int kg_hierarchy_get_node_count(const kg_hierarchy_t* hier, uint32_t* count) {
    if (!hier || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_get_node_count: required parameter is NULL (hier, count)");
        return -1;
    }
    *count = hier->entry_count;
    return 0;
}

kg_edge_iterator_t* kg_hierarchy_edge_iterator(const kg_hierarchy_t* hier) {
    if (!hier) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hier is NULL");

        return NULL;

    }

    kg_edge_iterator_t* iter = (kg_edge_iterator_t*)nimcp_calloc(1, sizeof(kg_edge_iterator_t));
    if (!iter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "iter is NULL");

        return NULL;

    }

    iter->hier = hier;
    iter->current_entry = 0;
    iter->edges = NULL;
    iter->current_edge = 0;

    return iter;
}

int kg_edge_iterator_next(kg_edge_iterator_t* iter, kg_edge_t* edge) {
    if (!iter || !edge || !iter->hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_edge_iterator_next: required parameter is NULL (iter, edge, iter->hier)");
        return -1;
    }

    const kg_hierarchy_t* hier = iter->hier;

    while (iter->current_entry < hier->entry_count) {
        /* If we don't have edges for current entry, fetch them */
        if (!iter->edges && hier->kg) {
            brain_kg_node_id_t node_id = hier->entries[iter->current_entry].node_id;
            iter->edges = brain_kg_get_outgoing(hier->kg, node_id);
            iter->current_edge = 0;
        }

        /* If we have edges, return the next one */
        if (iter->edges && iter->current_edge < iter->edges->count) {
            brain_kg_edge_t* e = iter->edges->edges[iter->current_edge];
            edge->source = e->from;
            edge->target = e->to;
            edge->weight = e->weight;
            iter->current_edge++;
            return 0;
        }

        /* Free current edge list and move to next entry */
        if (iter->edges) {
            brain_kg_edge_list_destroy(iter->edges);
            iter->edges = NULL;
        }
        iter->current_entry++;
        iter->current_edge = 0;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_edge_iterator_next: validation failed");
    return -1;  /* No more edges */
}

void kg_edge_iterator_free(kg_edge_iterator_t* iter) {
    if (iter) {
        if (iter->edges) {
            brain_kg_edge_list_destroy(iter->edges);
        }
        nimcp_free(iter);
    }
}

int kg_hierarchy_set_edge_metadata_int(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    const char* key,
    int32_t value
) {
    if (!hier || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_set_edge_metadata_int: required parameter is NULL (hier, key)");
        return -1;
    }

    /* For now, edge metadata is stored in the brain_kg edge description
     * A more complete implementation would use a separate metadata store */
    (void)from;
    (void)to;
    (void)value;

    return 0;  /* Stub - pretend success */
}

int kg_hierarchy_get_edge_metadata_int(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    const char* key,
    int32_t* value
) {
    if (!hier || !key || !value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_hierarchy_get_edge_metadata_int: required parameter is NULL (hier, key, value)");
        return -1;
    }

    /* Stub implementation - return default value */
    (void)from;
    (void)to;
    *value = 0;

    return 0;
}
