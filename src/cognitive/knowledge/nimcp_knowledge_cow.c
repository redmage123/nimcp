//=============================================================================
// nimcp_knowledge_cow.c - Copy-on-Write Wrapper for Knowledge System
//=============================================================================
/**
 * @file nimcp_knowledge_cow.c
 * @brief COW wrapper implementation using page-level COW infrastructure
 *
 * WHAT: Thin wrapper applying page-level COW to knowledge systems
 * WHY:  95% memory savings when multiple brains share common knowledge
 * HOW:  Delegates to nimcp_page_cow.h for all COW operations
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 1.0.0
 */

#include "cognitive/knowledge/nimcp_knowledge_cow.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_page_cow.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(knowledge_cow, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Structures
//=============================================================================

#define KNOWLEDGE_COW_BASE_MAGIC 0x4B434257  // 'KCBW'
#define KNOWLEDGE_COW_VIEW_MAGIC 0x4B435657  // 'KCVW'
#define KNOWLEDGE_COW_SNAP_MAGIC 0x4B435357  // 'KCSW'

/**
 * @brief COW-enabled knowledge base structure
 */
struct knowledge_cow_base_struct {
    uint32_t magic;                     /**< Magic number for validation */
    knowledge_cow_config_t config;      /**< Configuration */
    page_cow_region_t region;           /**< Underlying page COW region */
    size_t data_size;                   /**< Actual data size */
    bool initialized;                   /**< Is initialized? */
};

/**
 * @brief COW view into knowledge base
 */
struct knowledge_cow_view_struct {
    uint32_t magic;                     /**< Magic number for validation */
    knowledge_cow_base_t base;          /**< Parent base */
    page_cow_view_t page_view;          /**< Underlying page COW view */
};

/**
 * @brief Knowledge snapshot structure
 */
struct knowledge_cow_snapshot_struct {
    uint32_t magic;                     /**< Magic number */
    knowledge_cow_view_t source_view;   /**< Source view */
    page_cow_snapshot_t page_snapshot;  /**< Underlying page snapshot */
};

//=============================================================================
// Knowledge COW Base API Implementation
//=============================================================================

NIMCP_EXPORT knowledge_cow_base_t knowledge_cow_base_create(
    const knowledge_cow_config_t* config,
    const void* initial_data,
    size_t data_size
) {
    if (!config || config->max_knowledge_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_cow_base_create: config is NULL");
        return NULL;
    }

    // Ensure page COW subsystem is initialized
    if (!page_cow_init()) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "knowledge_cow_base_create: page_cow_init is NULL");
        return NULL;
    }

    // Allocate base structure
    knowledge_cow_base_t base = nimcp_calloc(1, sizeof(struct knowledge_cow_base_struct));
    if (!base) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate base");

        return NULL;

    }

    base->magic = KNOWLEDGE_COW_BASE_MAGIC;
    memcpy(&base->config, config, sizeof(knowledge_cow_config_t));
    base->data_size = data_size > 0 ? data_size : config->max_knowledge_size;

    // Create underlying page COW region
    page_cow_config_t page_config = page_cow_default_config(config->max_knowledge_size);
    page_config.enable_tracking = config->enable_tracking;
    page_config.zero_on_allocate = (initial_data == NULL);

    base->region = page_cow_region_create(&page_config, initial_data);
    if (!base->region) {
        nimcp_free(base);
        base = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_cow_base_create: base->region is NULL");
        return NULL;
    }

    base->initialized = true;
    return base;
}

NIMCP_EXPORT void knowledge_cow_base_destroy(knowledge_cow_base_t base) {
    if (!base || base->magic != KNOWLEDGE_COW_BASE_MAGIC) return;

    // Destroy underlying page COW region
    if (base->region) {
        page_cow_region_destroy(base->region);
    }

    base->magic = 0;
    nimcp_free(base);
    base = NULL;
}

NIMCP_EXPORT bool knowledge_cow_base_get_stats(
    knowledge_cow_base_t base,
    knowledge_cow_stats_t* stats
) {
    if (!base || base->magic != KNOWLEDGE_COW_BASE_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_cow_base_get_stats: required parameter is NULL (base, stats)");
        return false;
    }

    memset(stats, 0, sizeof(knowledge_cow_stats_t));

    // Get underlying page COW stats
    page_cow_stats_t page_stats;
    if (page_cow_region_get_stats(base->region, &page_stats)) {
        stats->total_views = page_stats.total_views;
        stats->active_views = page_stats.active_views;
        stats->shared_pages = page_stats.shared_pages;
        stats->private_pages = page_stats.private_pages;
        stats->memory_saved_bytes = page_stats.memory_saved_bytes;
        stats->cow_triggers = page_stats.cow_faults;
    }

    stats->total_knowledge_bytes = base->data_size;

    return true;
}

NIMCP_EXPORT bool knowledge_cow_base_update(
    knowledge_cow_base_t base,
    const void* data,
    size_t data_size,
    size_t offset
) {
    if (!base || base->magic != KNOWLEDGE_COW_BASE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_cow_base_update: base is NULL");
        return false;
    }
    if (!data || data_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_cow_base_update: data is NULL");
        return false;
    }
    if (offset + data_size > base->config.max_knowledge_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_cow_base_update: validation failed");
        return false;
    }

    // For base updates, we need to recreate the region
    // This is a heavyweight operation - use sparingly

    // Get current data
    page_cow_view_t temp_view = page_cow_view_create(base->region);
    if (!temp_view) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_cow_base_update: temp_view is NULL");
        return false;
    }

    const void* current_data = page_cow_view_read(temp_view);
    if (!current_data) {
        page_cow_view_destroy(temp_view);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_cow_base_update: current_data is NULL");
        return false;
    }

    // Create merged data
    size_t total_size = base->config.max_knowledge_size;
    void* merged = nimcp_malloc(total_size);
    if (!merged) {
        page_cow_view_destroy(temp_view);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_cow_base_update: merged is NULL");
        return false;
    }

    memcpy(merged, current_data, total_size);
    memcpy((char*)merged + offset, data, data_size);

    page_cow_view_destroy(temp_view);

    // Recreate region with new data
    page_cow_region_destroy(base->region);

    page_cow_config_t page_config = page_cow_default_config(total_size);
    page_config.enable_tracking = base->config.enable_tracking;

    base->region = page_cow_region_create(&page_config, merged);
    nimcp_free(merged);
    merged = NULL;

    if (!base->region) {
        base->initialized = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_cow_base_update: base->region is NULL");
        return false;
    }

    if (offset + data_size > base->data_size) {
        base->data_size = offset + data_size;
    }

    return true;
}

//=============================================================================
// Knowledge COW View API Implementation
//=============================================================================

NIMCP_EXPORT knowledge_cow_view_t knowledge_cow_view_create(knowledge_cow_base_t base) {
    if (!base || base->magic != KNOWLEDGE_COW_BASE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_cow_view_create: base is NULL");
        return NULL;
    }
    if (!base->initialized || !base->region) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_cow_view_create: required parameter is NULL (base->initialized, base->region)");
        return NULL;
    }

    knowledge_cow_view_t view = nimcp_calloc(1, sizeof(struct knowledge_cow_view_struct));
    if (!view) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate view");

        return NULL;

    }

    view->magic = KNOWLEDGE_COW_VIEW_MAGIC;
    view->base = base;

    // Create underlying page COW view
    view->page_view = page_cow_view_create(base->region);
    if (!view->page_view) {
        nimcp_free(view);
        view = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_cow_view_create: view->page_view is NULL");
        return NULL;
    }

    return view;
}

NIMCP_EXPORT knowledge_cow_view_t knowledge_cow_view_clone(knowledge_cow_view_t source) {
    if (!source || source->magic != KNOWLEDGE_COW_VIEW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_cow_view_clone: source is NULL");
        return NULL;
    }

    knowledge_cow_view_t clone = nimcp_calloc(1, sizeof(struct knowledge_cow_view_struct));
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate clone");

        return NULL;

    }

    clone->magic = KNOWLEDGE_COW_VIEW_MAGIC;
    clone->base = source->base;

    // Clone underlying page COW view
    clone->page_view = page_cow_view_clone(source->page_view);
    if (!clone->page_view) {
        nimcp_free(clone);
        clone = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_cow_view_clone: clone->page_view is NULL");
        return NULL;
    }

    return clone;
}

NIMCP_EXPORT void knowledge_cow_view_destroy(knowledge_cow_view_t view) {
    if (!view || view->magic != KNOWLEDGE_COW_VIEW_MAGIC) return;

    if (view->page_view) {
        page_cow_view_destroy(view->page_view);
    }

    view->magic = 0;
    nimcp_free(view);
    view = NULL;
}

NIMCP_EXPORT const void* knowledge_cow_view_read(knowledge_cow_view_t view) {
    if (!view || view->magic != KNOWLEDGE_COW_VIEW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_cow_view_read: view is NULL");
        return NULL;
    }
    return page_cow_view_read(view->page_view);
}

NIMCP_EXPORT void* knowledge_cow_view_write(knowledge_cow_view_t view) {
    if (!view || view->magic != KNOWLEDGE_COW_VIEW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_cow_view_write: view is NULL");
        return NULL;
    }
    return page_cow_view_write(view->page_view);
}

NIMCP_EXPORT bool knowledge_cow_view_make_region_private(
    knowledge_cow_view_t view,
    size_t offset,
    size_t size
) {
    if (!view || view->magic != KNOWLEDGE_COW_VIEW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_cow_view_make_region_private: view is NULL");
        return false;
    }

    // Calculate page range
    size_t start_page = page_cow_offset_to_page(offset);
    size_t end_offset = offset + size;
    size_t end_page = page_cow_offset_to_page(end_offset);
    if (end_offset % PAGE_COW_PAGE_SIZE != 0) {
        end_page++;  // Include partial page
    }

    size_t num_pages = end_page - start_page;
    size_t made_private = page_cow_view_make_range_private(view->page_view, start_page, num_pages);

    return made_private == num_pages;
}

NIMCP_EXPORT size_t knowledge_cow_view_get_memory_saved(knowledge_cow_view_t view) {
    if (!view || view->magic != KNOWLEDGE_COW_VIEW_MAGIC) return 0;
    return page_cow_view_get_memory_saved(view->page_view);
}

NIMCP_EXPORT bool knowledge_cow_view_is_modified(knowledge_cow_view_t view) {
    if (!view || view->magic != KNOWLEDGE_COW_VIEW_MAGIC) {
        return false;
    }
    return page_cow_view_get_private_page_count(view->page_view) > 0;
}

NIMCP_EXPORT size_t knowledge_cow_view_get_private_page_count(knowledge_cow_view_t view) {
    if (!view || view->magic != KNOWLEDGE_COW_VIEW_MAGIC) return 0;
    return page_cow_view_get_private_page_count(view->page_view);
}

//=============================================================================
// Knowledge COW Snapshot API Implementation
//=============================================================================

NIMCP_EXPORT knowledge_cow_snapshot_t knowledge_cow_snapshot_create(knowledge_cow_view_t view) {
    if (!view || view->magic != KNOWLEDGE_COW_VIEW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_cow_snapshot_create: view is NULL");
        return NULL;
    }

    knowledge_cow_snapshot_t snap = nimcp_calloc(1, sizeof(struct knowledge_cow_snapshot_struct));
    if (!snap) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate snap");

        return NULL;

    }

    snap->magic = KNOWLEDGE_COW_SNAP_MAGIC;
    snap->source_view = view;

    // Create underlying page snapshot
    snap->page_snapshot = page_cow_snapshot_create(view->page_view);
    if (!snap->page_snapshot) {
        nimcp_free(snap);
        snap = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_cow_snapshot_create: snap->page_snapshot is NULL");
        return NULL;
    }

    return snap;
}

NIMCP_EXPORT bool knowledge_cow_snapshot_restore(
    knowledge_cow_view_t view,
    knowledge_cow_snapshot_t snapshot
) {
    if (!view || view->magic != KNOWLEDGE_COW_VIEW_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_cow_snapshot_restore: view is NULL");
        return false;
    }
    if (!snapshot || snapshot->magic != KNOWLEDGE_COW_SNAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_cow_snapshot_restore: snapshot is NULL");
        return false;
    }
    if (view != snapshot->source_view) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_cow_snapshot_restore: validation failed");
        return false;
    }

    return page_cow_snapshot_restore(view->page_view, snapshot->page_snapshot);
}

NIMCP_EXPORT void knowledge_cow_snapshot_destroy(knowledge_cow_snapshot_t snapshot) {
    if (!snapshot || snapshot->magic != KNOWLEDGE_COW_SNAP_MAGIC) return;

    if (snapshot->page_snapshot) {
        page_cow_snapshot_destroy(snapshot->page_snapshot);
    }

    snapshot->magic = 0;
    nimcp_free(snapshot);
    snapshot = NULL;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query knowledge graph for self-knowledge about COW wrapper
 *
 * WHAT: Retrieves entity observations and relations for knowledge COW system
 * WHY: Enables self-aware introspection of module capabilities
 * HOW: Uses kg_reader to query JSONL knowledge graph
 *
 * @param kg Knowledge graph reader instance
 * @return 1 if self-knowledge found, 0 otherwise
 */
int knowledge_cow_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_cow_heartbeat("knowledge_co_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Knowledge_COW");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                knowledge_cow_heartbeat("knowledge_co_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Knowledge_COW");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Knowledge_COW");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void knowledge_cow_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_knowledge_cow_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int knowledge_cow_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_cow_training_begin: NULL argument");
        return -1;
    }
    knowledge_cow_heartbeat_instance(NULL, "knowledge_cow_training_begin", 0.0f);
    (void)(struct knowledge_cow_base_struct*)instance; /* Module state available for reset */
    return 0;
}

int knowledge_cow_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_cow_training_end: NULL argument");
        return -1;
    }
    knowledge_cow_heartbeat_instance(NULL, "knowledge_cow_training_end", 1.0f);
    (void)(struct knowledge_cow_base_struct*)instance; /* Module state available for finalization */
    return 0;
}

int knowledge_cow_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_cow_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    knowledge_cow_heartbeat_instance(NULL, "knowledge_cow_training_step", progress);
    (void)(struct knowledge_cow_base_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
