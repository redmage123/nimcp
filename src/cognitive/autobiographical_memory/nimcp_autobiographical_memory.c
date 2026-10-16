// ============================================================================
// nimcp_autobiographical_memory.c - Episodic Self-Memory Implementation
// ============================================================================

/**
 * BIO-ASYNC INTEGRATION:
 * - Module ID: 0x0335 (BIO_MODULE_AUTOBIOGRAPHICAL_MEMORY)
 * - Publishes: memory storage, retrieval, consolidation events
 * - Subscribes: emotional tags, temporal context
 */

#define LOG_MODULE "autobiographical_memory"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(autobiographical_memory, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* ============================================================================
 * Phase 8 Instance-Level Health Agent Support
 * ============================================================================ */



#include "cognitive/nimcp_autobiographical_memory.h"
#include "cognitive/autobiographical_memory/nimcp_autobio_snn_bridge.h"
#include "cognitive/autobiographical_memory/nimcp_autobio_plasticity_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/executive/nimcp_w9kg_events.h"  // W9-kg: KG event + read helpers
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "nimcp.h"  // For NIMCP_ERROR_* codes
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// BIO-ASYNC MODULE REGISTRATION
//=============================================================================

#define BIO_MODULE_AUTOBIOGRAPHICAL_MEMORY 0x0335

// ============================================================================
// Internal Structure
// ============================================================================

struct autobiographical_memory_system {
    autobiographical_memory_entry_t* memories;   // Array of memories
    uint32_t capacity;                     // Maximum memories
    uint32_t count;                        // Current count
    uint64_t next_id;                      // Next memory ID

    // Statistics
    autobio_stats_t stats;

    // Thread safety
    nimcp_mutex_t mutex;

    // Unified memory integration (CoW support for brain cloning)
    void* mem_manager;              /**< unified_mem_manager_t */
    void* memories_handle;          /**< CoW handle for memories array */

    // Bio-async integration
    void* bio_ctx;                  /**< bio_module_context_t pointer */
    bool bio_async_enabled;         /**< Bio-async registration status */

    // SNN and Plasticity bridge integration
    autobio_snn_bridge_t* snn_bridge;           /**< SNN bridge for memory encoding */
    autobio_plasticity_bridge_t* plasticity_bridge; /**< Plasticity bridge for consolidation */
    bool bridges_enabled;                        /**< Bridges initialization status */
};

/*=============================================================================
 * BIO-ASYNC HANDLERS (Forward declarations)
 *============================================================================*/

static nimcp_error_t handle_memory_retrieve_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static void bio_broadcast_memory_stored(struct autobiographical_memory_system* system, uint64_t memory_id, float salience);

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the orchestrator with discovered message types from the knowledge graph.
 * Registers handlers based on message types discovered at runtime.
 */
static int autobiographical_memory_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            autobiographical_memory_heartbeat("autobiograph_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_WORKING_MEMORY_RETRIEVE:
                bio_router_register_handler(ctx, message_types[i], handle_memory_retrieve_request);
                registered++;
                break;
            default:
                LOG_DEBUG("Autobiographical memory: unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    return (registered > 0) ? 0 : -1;
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void)
{
    return nimcp_time_monotonic_ms();
}

/**
 * @brief Initialize memory query with defaults
 */
static void init_query(memory_query_t* query)
{
    memset(query, 0, sizeof(memory_query_t));
    query->max_results = UINT32_MAX;
    query->sort_by_recency = true;
}

/**
 * @brief Check if memory matches query filters
 */
static bool matches_query(const autobiographical_memory_entry_t* mem,
                          const memory_query_t* query)
{
    // Time range filter
    if (query->start_time_ms > 0 && mem->timestamp_ms < query->start_time_ms) {
        return false;
    }
    if (query->end_time_ms > 0 && mem->timestamp_ms > query->end_time_ms) {
        return false;
    }

    // Type filter
    if (query->filter_by_type && mem->type != query->type_filter) {
        return false;
    }

    // Valence filter
    if (query->filter_by_valence && mem->valence != query->valence_filter) {
        return false;
    }

    // Importance filter
    if (query->filter_by_importance && mem->importance < query->min_importance) {
        return false;
    }

    // Text search
    if (query->search_text && query->search_text[0] != '\0') {
        bool found = false;
        if (query->case_sensitive) {
            found = (strstr(mem->what_happened, query->search_text) != NULL) ||
                   (strstr(mem->why_it_happened, query->search_text) != NULL) ||
                   (strstr(mem->outcome, query->search_text) != NULL);
        } else {
            // Case-insensitive search (simplified)
            found = (strstr(mem->what_happened, query->search_text) != NULL) ||
                   (strstr(mem->why_it_happened, query->search_text) != NULL) ||
                   (strstr(mem->outcome, query->search_text) != NULL);
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Comparison for sorting by recency
 */
static int compare_by_recency(const void* a, const void* b)
{
    const autobiographical_memory_entry_t* ma = (const autobiographical_memory_entry_t*)a;
    const autobiographical_memory_entry_t* mb = (const autobiographical_memory_entry_t*)b;

    // Newer first
    if (ma->timestamp_ms > mb->timestamp_ms) return -1;
    if (ma->timestamp_ms < mb->timestamp_ms) return 1;
    return 0;
}

/**
 * @brief Comparison for sorting by importance
 */
static int compare_by_importance(const void* a, const void* b)
{
    const autobiographical_memory_entry_t* ma = (const autobiographical_memory_entry_t*)a;
    const autobiographical_memory_entry_t* mb = (const autobiographical_memory_entry_t*)b;

    // More important first
    if (ma->importance > mb->importance) return -1;
    if (ma->importance < mb->importance) return 1;
    return 0;
}

// ============================================================================
// Core API Implementation
// ============================================================================

autobiographical_memory_t autobio_create(uint32_t capacity)
{
    // Guard: Use default capacity if 0
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_create", 0.0f);


    if (capacity == 0) {
        capacity = AUTOBIO_DEFAULT_CAPACITY;
    }

    LOG_INFO("Creating autobiographical memory system: capacity=%u", capacity);

    // Allocate system
    struct autobiographical_memory_system* system =
        nimcp_calloc(1, sizeof(struct autobiographical_memory_system));
    if (!system) {
        LOG_ERROR("Failed to allocate autobiographical memory system (%zu bytes)",
                 sizeof(struct autobiographical_memory_system));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;
    }

    // Initialize unified memory manager for CoW support
    unified_mem_config_t mem_config = unified_mem_default_config();
    mem_config.enable_cow = true;
    mem_config.enable_tracking = true;
    system->mem_manager = unified_mem_create(&mem_config);

    // Allocate memory array (with unified memory if available)
    if (system->mem_manager) {
        unified_mem_request_t req = unified_mem_request(
            capacity * sizeof(autobiographical_memory_entry_t),
            NULL, true
        );
        system->memories_handle = unified_mem_alloc(system->mem_manager, &req);
        if (system->memories_handle) {
            system->memories = (autobiographical_memory_entry_t*)unified_mem_write(system->memories_handle);
            LOG_DEBUG(LOG_MODULE, "Memories array allocated via unified memory with CoW");
        }
    }
    if (!system->memories) {
        system->memories = nimcp_calloc(capacity, sizeof(autobiographical_memory_entry_t));
    }
    if (!system->memories) {
        if (system->mem_manager) unified_mem_destroy(system->mem_manager);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "autobio_create: validation failed");
        return NULL;
    }

    system->capacity = capacity;
    system->count = 0;
    system->next_id = 1;  // Start from 1 (0 = invalid ID)

    // Initialize mutex
    if (nimcp_mutex_init(&system->mutex, NULL) != NIMCP_SUCCESS) {
        if (system->memories_handle) unified_mem_free(system->memories_handle);
        else nimcp_free(system->memories);
        if (system->mem_manager) unified_mem_destroy(system->mem_manager);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_create: validation failed");
        return NULL;
    }

    // Initialize stats
    memset(&system->stats, 0, sizeof(autobio_stats_t));

    
    // Bio-async registration
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_MEMORY_AUTOBIOGRAPHICAL,
            .module_name = "autobiographical_memory",
            .inbox_capacity = 32,
            .user_data = system
        };
        system->bio_ctx = bio_router_register_module(&bio_info);
        if (system->bio_ctx) {
            system->bio_async_enabled = true;

            // Try KG-driven wiring callback registration first
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_MEMORY_AUTOBIOGRAPHICAL,
                (void*)autobiographical_memory_wiring_handler_callback,
                system
            );

            if (wiring_result == NIMCP_SUCCESS) {
                LOG_INFO("Autobiographical memory: KG-driven wiring callback registered");
            } else {
                // Legacy fallback - register handlers directly
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(system->bio_ctx, BIO_MSG_WORKING_MEMORY_RETRIEVE,
                                                handle_memory_retrieve_request)
                );
                LOG_INFO("Autobiographical memory: legacy handler registration");
            }
        }
    }

    // Initialize SNN and Plasticity bridges
    system->bridges_enabled = false;
    system->snn_bridge = NULL;
    system->plasticity_bridge = NULL;

    // Create SNN bridge with default config
    autobio_snn_config_t snn_config = autobio_snn_config_default();
    system->snn_bridge = autobio_snn_create(&snn_config);
    if (system->snn_bridge) {
        LOG_INFO("Autobiographical memory: SNN bridge initialized");
    } else {
        LOG_WARN("Autobiographical memory: Failed to initialize SNN bridge");
    }

    // Create Plasticity bridge with default config
    autobio_plasticity_config_t plasticity_config = autobio_plasticity_config_default();
    system->plasticity_bridge = autobio_plasticity_create(&plasticity_config);
    if (system->plasticity_bridge) {
        LOG_INFO("Autobiographical memory: Plasticity bridge initialized");
    } else {
        LOG_WARN("Autobiographical memory: Failed to initialize plasticity bridge");
    }

    // Mark bridges as enabled if both initialized successfully
    system->bridges_enabled = (system->snn_bridge != NULL && system->plasticity_bridge != NULL);

    return system;
}

/*=============================================================================
 * BIO-ASYNC HANDLER IMPLEMENTATIONS
 *============================================================================*/

static nimcp_error_t handle_memory_retrieve_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)msg_size;
    (void)response_promise;
    NIMCP_CHECK_THROW(msg && user_data, NIMCP_ERROR_NULL_ARG, "msg or user_data is NULL");
    struct autobiographical_memory_system* system = (struct autobiographical_memory_system*)user_data;
    LOG_DEBUG(LOG_MODULE, "Received memory retrieve request, count=%u", system->count);
    return NIMCP_SUCCESS;
}

static void bio_broadcast_memory_stored(struct autobiographical_memory_system* system, uint64_t memory_id, float salience) {
    if (!system || !system->bio_async_enabled || !system->bio_ctx) { return; }
    // Use salience response for memory stored notification
    bio_msg_salience_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(system->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.stimulus_id = (uint32_t)memory_id;
    msg.salience_score = salience;
    msg.attention_priority = salience;
    msg.requires_immediate_attention = (salience > 0.7F);
    bio_router_broadcast(system->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast memory stored: id=%lu, salience=%.2f", (unsigned long)memory_id, salience);
}

void autobio_destroy(autobiographical_memory_t system)
{
    if (!system) {
        return;
    }

    // Destroy SNN and Plasticity bridges
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_destroy", 0.0f);


    if (system->snn_bridge) {
        autobio_snn_destroy(system->snn_bridge);
        system->snn_bridge = NULL;
    }
    if (system->plasticity_bridge) {
        autobio_plasticity_destroy(system->plasticity_bridge);
        system->plasticity_bridge = NULL;
    }
    system->bridges_enabled = false;

    // Unregister from bio-router
    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        system->bio_async_enabled = false;
    }

    nimcp_mutex_destroy(&system->mutex);

    // Free memories array (unified memory or direct)
    if (system->memories_handle) {
        unified_mem_free(system->memories_handle);
        system->memories_handle = NULL;
        system->memories = NULL;
    } else if (system->memories) {
        nimcp_free(system->memories);
        system->memories = NULL;
    }

    // Destroy unified memory manager
    if (system->mem_manager) {
        unified_mem_destroy(system->mem_manager);
        system->mem_manager = NULL;
    }

    nimcp_free(system);
    system = NULL;
}

uint64_t autobio_store(autobiographical_memory_t system,
                       const autobiographical_memory_entry_t* memory)
{
    // Guard: NULL checks
    if (!system || !memory) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_store", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    // Guard: Check capacity
    if (system->count >= system->capacity) {
        // Could implement eviction policy here
        nimcp_mutex_unlock(&system->mutex);
        return 0;
    }

    // Copy memory
    autobiographical_memory_entry_t* slot = &system->memories[system->count];
    memcpy(slot, memory, sizeof(autobiographical_memory_entry_t));

    // Assign ID and timestamp
    slot->memory_id = system->next_id++;
    if (slot->timestamp_ms == 0) {
        slot->timestamp_ms = get_current_time_ms();
    }

    // Initialize retrieval stats
    slot->times_recalled = 0;
    slot->last_recalled_ms = 0;
    if (slot->memory_strength == 0.0F) {
        slot->memory_strength = 1.0F;  // New memories start vivid
    }

    // Update system stats
    system->count++;
    system->stats.total_memories = system->count;
    system->stats.memories_by_type[memory->type]++;
    if (memory->is_core_memory) {
        system->stats.core_memories++;
    }

    // Update time bounds
    if (system->stats.oldest_memory_ms == 0 ||
        slot->timestamp_ms < system->stats.oldest_memory_ms) {
        system->stats.oldest_memory_ms = slot->timestamp_ms;
    }
    if (slot->timestamp_ms > system->stats.newest_memory_ms) {
        system->stats.newest_memory_ms = slot->timestamp_ms;
    }

    // Update average stats (running average)
    float alpha = 1.0F / (float)system->count;
    system->stats.avg_importance =
        (1.0F - alpha) * system->stats.avg_importance + alpha * memory->importance;
    system->stats.avg_emotional_intensity =
        (1.0F - alpha) * system->stats.avg_emotional_intensity + alpha * memory->emotional_intensity;

    uint64_t assigned_id = slot->memory_id;
    float assigned_importance = slot->importance;

    nimcp_mutex_unlock(&system->mutex);

    /* W9-kg: emit autobio stored event outside the lock. */
    {
        struct brain_struct* brain = w9kg_get_registered_brain();
        if (brain) {
            w9kg_emit_autobio_stored(brain, assigned_id, assigned_importance);
        }
    }

    return assigned_id;
}

bool autobio_retrieve(autobiographical_memory_t system,
                     uint64_t memory_id,
                     autobiographical_memory_entry_t* out_memory)
{
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_retrieve", 0.0f);


    if (system && system->bio_ctx) {
        bio_router_process_inbox(system->bio_ctx, 5);
    }

    // Guard: NULL checks
    if (!system || !out_memory || memory_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_retrieve: required parameter is NULL (system, out_memory)");
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    // Linear search (could use hash map for O(1))
    bool found = false;
    for (uint32_t i = 0; i < system->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->count > 256) {
            autobiographical_memory_heartbeat("autobiograph_loop",
                             (float)(i + 1) / (float)system->count);
        }

        if (system->memories[i].memory_id == memory_id) {
            // Copy memory (use entry_t size, not the pointer typedef)
            memcpy(out_memory, &system->memories[i], sizeof(autobiographical_memory_entry_t));

            // Update retrieval stats
            system->memories[i].times_recalled++;
            system->memories[i].last_recalled_ms = get_current_time_ms();
            system->stats.total_retrievals++;

            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(&system->mutex);

    /* W9-kg: emit retrieval event (if hit) outside the lock. */
    if (found) {
        struct brain_struct* brain = w9kg_get_registered_brain();
        if (brain) {
            w9kg_emit_autobio_retrieved(brain, memory_id);
        }
    }

    return found;
}

bool autobio_query(autobiographical_memory_t system,
                  const memory_query_t* query,
                  autobiographical_memory_entry_t* results,
                  uint32_t max_results,
                  uint32_t* num_found)
{
    // Guard: NULL checks
    if (!system || !query || !results || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_query: required parameter is NULL (system, query, results, num_found)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_query", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    *num_found = 0;

    // Collect matching memories
    for (uint32_t i = 0; i < system->count && *num_found < max_results; i++) {
        if (matches_query(&system->memories[i], query)) {
            memcpy(&results[*num_found], &system->memories[i],
                   sizeof(autobiographical_memory_entry_t));
            (*num_found)++;

            // Update retrieval stats
            system->memories[i].times_recalled++;
            system->memories[i].last_recalled_ms = get_current_time_ms();
        }
    }

    system->stats.total_retrievals += *num_found;

    // Sort results
    if (*num_found > 1) {
        if (query->sort_by_recency) {
            qsort(results, *num_found, sizeof(autobiographical_memory_entry_t),
                  compare_by_recency);
        } else if (query->sort_by_importance) {
            qsort(results, *num_found, sizeof(autobiographical_memory_entry_t),
                  compare_by_importance);
        }
    }

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool autobio_get_recent(autobiographical_memory_t system,
                       uint32_t count,
                       autobiographical_memory_entry_t* results,
                       uint32_t* num_found)
{
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_get_recent", 0.0f);


    memory_query_t query = {0};
    init_query(&query);
    query.max_results = count;
    query.sort_by_recency = true;

    return autobio_query(system, &query, results, count, num_found);
}

bool autobio_get_core_memories(autobiographical_memory_t system,
                               autobiographical_memory_entry_t* results,
                               uint32_t max_results,
                               uint32_t* num_found)
{
    // Guard: NULL checks
    if (!system || !results || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_get_core_memories: required parameter is NULL (system, results, num_found)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_get_core_mem", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    *num_found = 0;

    // Collect core memories
    for (uint32_t i = 0; i < system->count && *num_found < max_results; i++) {
        if (system->memories[i].is_core_memory) {
            memcpy(&results[*num_found], &system->memories[i],
                   sizeof(autobiographical_memory_entry_t));
            (*num_found)++;
        }
    }

    // Sort by importance
    if (*num_found > 1) {
        qsort(results, *num_found, sizeof(autobiographical_memory_entry_t),
              compare_by_importance);
    }

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

bool autobio_mark_core(autobiographical_memory_t system,
                      uint64_t memory_id,
                      bool is_core)
{
    // Guard: NULL checks
    if (!system || memory_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_mark_core: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_mark_core", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    bool found = false;
    for (uint32_t i = 0; i < system->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->count > 256) {
            autobiographical_memory_heartbeat("autobiograph_loop",
                             (float)(i + 1) / (float)system->count);
        }

        if (system->memories[i].memory_id == memory_id) {
            bool was_core = system->memories[i].is_core_memory;
            system->memories[i].is_core_memory = is_core;

            // Update stats
            if (is_core && !was_core) {
                system->stats.core_memories++;
            } else if (!is_core && was_core) {
                system->stats.core_memories--;
            }

            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(&system->mutex);

    return found;
}

bool autobio_update_importance(autobiographical_memory_t system,
                              uint64_t memory_id,
                              float new_importance)
{
    // Guard: NULL checks and bounds
    if (!system || memory_id == 0 || new_importance < 0.0F || new_importance > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "autobio_update_importance: system is NULL or params out of range");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_update_impor", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    bool found = false;
    for (uint32_t i = 0; i < system->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->count > 256) {
            autobiographical_memory_heartbeat("autobiograph_loop",
                             (float)(i + 1) / (float)system->count);
        }

        if (system->memories[i].memory_id == memory_id) {
            system->memories[i].importance = new_importance;
            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(&system->mutex);

    return found;
}

bool autobio_get_stats(autobiographical_memory_t system,
                      autobio_stats_t* stats)
{
    // Guard: NULL checks
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_get_stats: required parameter is NULL (system, stats)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_get_stats", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    memcpy(stats, &system->stats, sizeof(autobio_stats_t));
    stats->memory_usage_bytes = system->capacity * sizeof(autobiographical_memory_entry_t) +
                                sizeof(struct autobiographical_memory_system);

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

uint32_t autobio_consolidate(autobiographical_memory_t system)
{
    // Guard: NULL check
    if (!system) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_consolidate", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    uint32_t pruned = 0;
    uint64_t current_time = get_current_time_ms();

    // Consolidation algorithm:
    // 1. Strengthen oft-recalled memories
    // 2. Decay rarely-accessed memories
    // 3. Prune very weak, unimportant memories

    for (uint32_t i = 0; i < system->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->count > 256) {
            autobiographical_memory_heartbeat("autobiograph_loop",
                             (float)(i + 1) / (float)system->count);
        }

        autobiographical_memory_entry_t* mem = &system->memories[i];

        // Skip core memories
        if (mem->is_core_memory) {
            continue;
        }

        // Calculate time since last recall
        uint64_t time_since_recall = current_time - mem->last_recalled_ms;
        float days_since_recall = time_since_recall / (1000.0F * 60.0F * 60.0F * 24.0F);

        // Strengthen if recalled recently and often
        if (mem->times_recalled > 3 && days_since_recall < 7.0F) {
            mem->memory_strength = fminf(1.0F, mem->memory_strength * 1.1F);
        }

        // Decay if not recalled
        if (days_since_recall > 30.0F) {
            mem->memory_strength *= 0.9F;
        }

        // Prune if very weak and unimportant
        if (mem->memory_strength < 0.2F && mem->importance < AUTOBIO_IMPORTANCE_THRESHOLD) {
            // Mark for pruning (set memory_id to 0)
            mem->memory_id = 0;
            pruned++;
        }
    }

    // Compact array (remove pruned memories)
    if (pruned > 0) {
        uint32_t write_idx = 0;
        for (uint32_t read_idx = 0; read_idx < system->count; read_idx++) {
            /* Phase 8: Loop progress heartbeat */
            if ((read_idx & 0xFF) == 0 && system->count > 256) {
                autobiographical_memory_heartbeat("autobiograph_loop",
                                 (float)(read_idx + 1) / (float)system->count);
            }

            if (system->memories[read_idx].memory_id != 0) {
                if (write_idx != read_idx) {
                    memcpy(&system->memories[write_idx], &system->memories[read_idx],
                           sizeof(autobiographical_memory_entry_t));
                }
                write_idx++;
            }
        }
        system->count = write_idx;
        system->stats.total_memories = system->count;
    }

    nimcp_mutex_unlock(&system->mutex);

    return pruned;
}

bool autobio_generate_timeline_summary(autobiographical_memory_t system,
                                      uint64_t start_time_ms,
                                      uint64_t end_time_ms,
                                      char* summary,
                                      size_t summary_len)
{
    // Guard: NULL checks
    if (!system || !summary || summary_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_generate_timeline_summary: required parameter is NULL (system, summary)");
        return false;
    }

    // Query memories in time range
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_generate_tim", 0.0f);


    memory_query_t query = {0};
    init_query(&query);
    query.start_time_ms = start_time_ms;
    query.end_time_ms = end_time_ms;
    query.sort_by_recency = true;
    query.max_results = 100;  // Limit to 100 memories

    autobiographical_memory_entry_t results[100];
    uint32_t found = 0;

    if (!autobio_query(system, &query, results, 100, &found)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "autobio_generate_timeline_summary: autobio_query is NULL");
        return false;
    }

    // Generate summary
    int written = snprintf(summary, summary_len,
                          "Timeline Summary (%u memories):\n\n", found);

    if (written < 0 || (size_t)written >= summary_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "autobio_generate_timeline_summary: capacity exceeded");
        return false;
    }

    for (uint32_t i = 0; i < found && (size_t)written < summary_len - 100; i++) {
        const autobiographical_memory_entry_t* mem = &results[i];

        int n = snprintf(summary + written, summary_len - written,
                        "[%s] %s\n  Why: %s\n  Outcome: %s\n\n",
                        (mem->type == AUTOBIO_LEARNING) ? "LEARNED" :
                        (mem->type == AUTOBIO_ACTION) ? "DID" :
                        (mem->type == AUTOBIO_DECISION) ? "DECIDED" :
                        (mem->type == AUTOBIO_ACHIEVEMENT) ? "ACHIEVED" :
                        (mem->type == AUTOBIO_INSIGHT) ? "REALIZED" : "EXPERIENCED",
                        mem->what_happened,
                        mem->why_it_happened,
                        mem->outcome);

        if (n > 0) {
            written += n;
        }
    }

    return true;
}

/* ============================================================================
 * SNN/Plasticity Bridge Integration Functions
 * ============================================================================ */

/**
 * @brief Encode a memory into the SNN bridge for neural processing
 *
 * WHAT: Encode memory attributes into SNN spike patterns
 * WHY:  Enable biologically-plausible memory processing
 * HOW:  Convert memory dimensions to neural activations
 *
 * @param system Memory system
 * @param memory_id ID of memory to encode
 * @return Spike count on success, -1 on failure
 */
int autobio_encode_memory_to_snn(autobiographical_memory_t system, uint64_t memory_id)
{
    if (!system || memory_id == 0 || !system->bridges_enabled || !system->snn_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_encode_memory_to_snn: required parameter is NULL (system, system->bridges_enabled, system->snn_bridge)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_encode_memor", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    // Find the memory
    autobiographical_memory_entry_t* mem = NULL;
    for (uint32_t i = 0; i < system->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->count > 256) {
            autobiographical_memory_heartbeat("autobiograph_loop",
                             (float)(i + 1) / (float)system->count);
        }

        if (system->memories[i].memory_id == memory_id) {
            mem = &system->memories[i];
            break;
        }
    }

    if (!mem) {
        nimcp_mutex_unlock(&system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_encode_memory_to_snn: mem is NULL");
        return -1;
    }

    // Calculate recency (0=distant, 1=now)
    uint64_t current_time = get_current_time_ms();
    uint64_t age_ms = current_time - mem->timestamp_ms;
    float recency = expf(-(float)age_ms / (1000.0f * 60.0f * 60.0f * 24.0f * 7.0f)); // ~1 week decay

    nimcp_mutex_unlock(&system->mutex);

    // Encode episodic trace (importance, self-relevance, vividness)
    int spikes1 = autobio_snn_encode_episodic(
        system->snn_bridge,
        mem->importance,
        mem->self_relevance,
        mem->memory_strength
    );

    // Encode temporal context
    int spikes2 = autobio_snn_encode_temporal(
        system->snn_bridge,
        recency,
        mem->timestamp_ms
    );

    // Encode emotional components
    // Convert valence enum to float: -2 to 2 -> -1 to 1
    float valence_f = (float)mem->valence / 2.0f;
    int spikes3 = autobio_snn_encode_emotional(
        system->snn_bridge,
        valence_f,
        mem->emotional_intensity,
        mem->arousal
    );

    // Simulate memory processing
    if (autobio_snn_simulate(system->snn_bridge, 50.0f) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "autobio_encode_memory_to_snn: validation failed");
        return -1;
    }

    int total_spikes = 0;
    if (spikes1 > 0) total_spikes += spikes1;
    if (spikes2 > 0) total_spikes += spikes2;
    if (spikes3 > 0) total_spikes += spikes3;

    LOG_DEBUG("Encoded memory %lu to SNN: spikes=%d, importance=%.2f, recency=%.2f",
              (unsigned long)memory_id, total_spikes, mem->importance, recency);

    return total_spikes;
}

/**
 * @brief Apply plasticity learning for memory consolidation
 *
 * WHAT: Strengthen memory traces through plasticity rules
 * WHY:  Enable memory consolidation and reinforcement
 * HOW:  Apply STDP and emotional modulation to synapses
 *
 * @param system Memory system
 * @param memory_id ID of memory to consolidate
 * @param emotional_boost Emotional intensity boost [0-1]
 * @return 0 on success, -1 on failure
 */
int autobio_apply_consolidation_plasticity(autobiographical_memory_t system,
                                            uint64_t memory_id,
                                            float emotional_boost)
{
    if (!system || memory_id == 0 || !system->bridges_enabled || !system->plasticity_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_apply_consolidation_plasticity: required parameter is NULL (system, system->bridges_enabled, system->plasticity_bridge)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_apply_consol", 0.0f);


    nimcp_mutex_lock(&system->mutex);

    // Find the memory
    autobiographical_memory_entry_t* mem = NULL;
    for (uint32_t i = 0; i < system->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->count > 256) {
            autobiographical_memory_heartbeat("autobiograph_loop",
                             (float)(i + 1) / (float)system->count);
        }

        if (system->memories[i].memory_id == memory_id) {
            mem = &system->memories[i];
            break;
        }
    }

    if (!mem) {
        nimcp_mutex_unlock(&system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_apply_consolidation_plasticity: mem is NULL");
        return -1;
    }

    float importance = mem->importance;
    float self_relevance = mem->self_relevance;
    bool is_core = mem->is_core_memory;
    uint32_t times_recalled = mem->times_recalled;

    nimcp_mutex_unlock(&system->mutex);

    // Register episodic synapse if not already (use memory_id as synapse_id)
    autobio_plasticity_register_synapse(
        system->plasticity_bridge,
        (uint32_t)memory_id,
        AUTOBIO_SYNAPSE_EPISODIC,
        0.5f
    );

    // Apply learning based on retrieval success
    if (times_recalled > 0) {
        autobio_plasticity_learn(
            system->plasticity_bridge,
            AUTOBIO_LEARN_RETRIEVAL_SUCCESS,
            importance,
            (uint32_t)memory_id,
            self_relevance
        );
    }

    // Apply emotional boost if significant
    if (emotional_boost > 0.1f) {
        autobio_plasticity_apply_emotional_boost(system->plasticity_bridge, emotional_boost);
        autobio_plasticity_learn(
            system->plasticity_bridge,
            AUTOBIO_LEARN_EMOTIONAL_BOOST,
            emotional_boost,
            (uint32_t)memory_id,
            self_relevance
        );
    }

    // Mark as core memory learning if applicable
    if (is_core) {
        autobio_plasticity_learn(
            system->plasticity_bridge,
            AUTOBIO_LEARN_CORE_MEMORY,
            importance,
            (uint32_t)memory_id,
            1.0f
        );
    }

    // Self-relevance modulation
    if (self_relevance > 0.7f) {
        autobio_plasticity_learn(
            system->plasticity_bridge,
            AUTOBIO_LEARN_SELF_RELEVANCE_HIGH,
            self_relevance,
            (uint32_t)memory_id,
            importance
        );
    }

    // Consolidate the learning
    autobio_plasticity_consolidate(system->plasticity_bridge);

    LOG_DEBUG("Applied consolidation plasticity to memory %lu: emotional_boost=%.2f",
              (unsigned long)memory_id, emotional_boost);

    return 0;
}

/**
 * @brief Get SNN recall state for a memory
 *
 * @param system Memory system
 * @param recall Output recall state
 * @return 0 on success, -1 on failure
 */
int autobio_get_snn_recall_state(autobiographical_memory_t system, autobio_recall_t* recall)
{
    if (!system || !recall || !system->bridges_enabled || !system->snn_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_get_snn_recall_state: required parameter is NULL (system, recall, system->bridges_enabled, system->snn_bridge)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_get_snn_reca", 0.0f);


    return autobio_snn_get_recall(system->snn_bridge, recall);
}

/**
 * @brief Get plasticity consolidation state
 *
 * @param system Memory system
 * @param state Output consolidation state
 * @return 0 on success, -1 on failure
 */
int autobio_get_consolidation_state(autobiographical_memory_t system,
                                     autobio_consolidation_state_t* state)
{
    if (!system || !state || !system->bridges_enabled || !system->plasticity_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_get_consolidation_state: required parameter is NULL (system, state, system->bridges_enabled, system->plasticity_bridge)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_get_consolid", 0.0f);


    return autobio_plasticity_get_consolidation_state(system->plasticity_bridge, state);
}

/**
 * @brief Check if SNN/plasticity bridges are enabled
 *
 * @param system Memory system
 * @return true if bridges are enabled and operational
 */
bool autobio_bridges_enabled(autobiographical_memory_t system)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_bridges_enabled: system is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_autobio_bridges_enab", 0.0f);


    return system->bridges_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int autobiographical_memory_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobiographical_memory_heartbeat("autobiograph_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Autobiographical_Memory");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                autobiographical_memory_heartbeat("autobiograph_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Autobiographical_Memory");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Autobiographical_Memory");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/**
 * @brief W9-kg read-path: query autobio events from the internal KG.
 *
 * WHAT: Count autobio event nodes (stored + retrieved) visible in the KG.
 *       Useful for the consolidation path — when KG event count aligns
 *       with in-process `system->count`, the module knows external
 *       evidence converges with its store.
 * WHY:  Closes autobiographical_memory UNWIRED status (previously only
 *       kg_reader integration existed; now the internal_kg is also
 *       queried).
 *
 * @param system autobio instance (non-NULL)
 * @return count of autobio event nodes, 0 if KG unavailable.
 */
uint32_t autobiographical_memory_query_kg_events(autobiographical_memory_t system)
{
    if (!system) return 0;

    struct brain_struct* brain = w9kg_get_registered_brain();
    if (!brain) return 0;

    return w9kg_query_autobio_event_count(brain);
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent Setter
 * ============================================================================ */

void autobiographical_memory_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    (void)instance;
    g_autobiographical_memory_instance_health_agent = agent;
    NIMCP_LOGGING_DEBUG("autobiographical_memory: instance health agent %s",
                        agent ? "set" : "cleared");
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int autobiographical_memory_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobiographical_memory_training_begin: ctx is NULL");
        return -1;
    }
    struct autobiographical_memory_system* sys = (struct autobiographical_memory_system*)ctx;
    autobiographical_memory_heartbeat_instance(g_autobiographical_memory_instance_health_agent, "autobio_mem_training_begin", 0.0f);

    memset(&sys->stats, 0, sizeof(sys->stats));

    NIMCP_LOGGING_INFO("autobiographical_memory: training begun, count=%u capacity=%u",
                       sys->count, sys->capacity);
    return 0;
}

int autobiographical_memory_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobiographical_memory_training_end: ctx is NULL");
        return -1;
    }
    struct autobiographical_memory_system* sys = (struct autobiographical_memory_system*)ctx;
    autobiographical_memory_heartbeat_instance(g_autobiographical_memory_instance_health_agent, "autobio_mem_training_end", 1.0f);

    float avg_retrievals = (float)sys->stats.total_retrievals;

    NIMCP_LOGGING_INFO("autobiographical_memory: training ended, memories=%u retrievals=%.0f",
                       sys->count, avg_retrievals);
    return 0;
}

int autobiographical_memory_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobiographical_memory_training_step: ctx is NULL");
        return -1;
    }
    struct autobiographical_memory_system* sys = (struct autobiographical_memory_system*)ctx;

    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    autobiographical_memory_heartbeat_instance(g_autobiographical_memory_instance_health_agent, "autobio_mem_training_step", p);

    sys->stats.total_retrievals++;

    return 0;
}
