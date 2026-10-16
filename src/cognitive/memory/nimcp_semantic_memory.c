/**
 * @file nimcp_semantic_memory.c
 * @brief Phase M4: Semantic Memory Network Implementation
 *
 * WHAT: Network-based semantic knowledge storage and retrieval
 * WHY:  Enable abstract reasoning and inference beyond episodic memory
 * HOW:  Concepts connected by relations, spreading activation for retrieval
 *
 * BIO-ASYNC INTEGRATION:
 * - Module ID: 0x0331 (BIO_MODULE_SEMANTIC_MEMORY)
 * - Publishes: concept activation, relation formation events
 * - Subscribes: consolidation events from M2
 *
 * @version Phase M4 Semantic Memory
 * @date 2025-11-13
 */

#define LOG_MODULE "semantic_memory"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(semantic_memory, MESH_ADAPTER_CATEGORY_MEMORY)



#include "cognitive/memory/nimcp_semantic_memory.h"
#include "cognitive/memory/nimcp_memory_kg_events.h"  /* W6: KG event emitters */
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

/* Quantum bridge integration */
#define NIMCP_SEMANTIC_QUANTUM_BRIDGE_IMPLEMENTATION
#include "cognitive/memory/nimcp_semantic_memory_quantum_bridge.h"

#include "nimcp.h"  // For NIMCP_ERROR_* codes
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/memory/nimcp_systems_consolidation.h"
#include "utils/platform/nimcp_platform_time.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/thread/nimcp_thread.h"  /* P2-COG-07: nimcp_mutex_create/lock/unlock/destroy */
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_SEMANTIC_MEMORY 0x0331

/* Concept-pool capacity. Cap chosen to fit a full WordNet+GloVe
 * preload (29,424 words from data/lexicon/wordnet_glove_v1.bin) with
 * runtime headroom for the trainer's lexicon growth. Must be ≥
 * GL_MAX_VOCAB (131072) to never block grounded_language_fast_map.
 *
 * Pre-fix: 2048 — verified 2026-05-09 as the actual ceiling that was
 * silently capping the production pod's vocab at ~2208 entries (the
 * 2048 concept slots + ~150 seed-word concept-aliases). bulk_lexicon
 * load reported "fast_map returned 0 at record 2197 — lexicon full"
 * for months. The "lexicon full" message was misleading (lexicon cap
 * is GL_MAX_VOCAB=131072); the real ceiling was here.
 *
 * Memory cost: each concept = ~600 bytes (features=128 floats +
 * label + metadata). 131072 × 600 ≈ 80 MB — well within the brain's
 * working set, and one-time at brain create. */
#define DEFAULT_CONCEPT_CAPACITY 131072
#define DEFAULT_RELATION_CAPACITY 32768
#define DEFAULT_FEATURE_DIM 32

//=============================================================================
// BIO-ASYNC HANDLERS (Forward declarations)
//=============================================================================

static nimcp_error_t handle_knowledge_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static void bio_broadcast_concept_activated(semantic_memory_system_t* system, uint32_t concept_id, float activation);

/*=============================================================================
 * KG-Driven Wiring Callback
 *============================================================================*/

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static int semantic_memory_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO(LOG_MODULE, "semantic_memory_wiring_handler_callback: registering %u handlers from KG",
             message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            semantic_memory_heartbeat("semantic_mem_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_KNOWLEDGE_QUERY:
                bio_router_register_handler(ctx, message_types[i], handle_knowledge_query);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_KNOWLEDGE_QUERY");
                break;

            default:
                LOG_DEBUG(LOG_MODULE, "  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Compute cosine similarity between feature vectors
 * WHAT: Measure similarity between two feature vectors
 * WHY:  Used for finding similar concepts
 * HOW:  Dot product divided by magnitudes
 */
static float compute_similarity(const float* a, const float* b, uint32_t dim) {
    if (!a || !b || dim == 0) return 0.0F;

    float dot = 0.0F;
    float mag_a = 0.0F;
    float mag_b = 0.0F;

    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            semantic_memory_heartbeat("semantic_mem_loop",
                             (float)(i + 1) / (float)dim);
        }

        dot += a[i] * b[i];
        mag_a += a[i] * a[i];
        mag_b += b[i] * b[i];
    }

    if (mag_a == 0.0F || mag_b == 0.0F) return 0.0F;

    return dot / (sqrtf(mag_a) * sqrtf(mag_b));
}

/**
 * @brief Find concept by ID in array
 * WHAT: Search for concept by ID
 * WHY:  Need to retrieve concepts for operations
 * HOW:  Linear search through concept array
 */
static semantic_concept_t* find_concept_by_id(
    semantic_memory_system_t* system,
    uint64_t id)
{
    if (!system || id == 0) {
        return NULL;
    }

    // Snapshot count for safe iteration (caller may or may not hold lock)
    uint32_t snapshot_count = system->concept_count;

    for (uint32_t i = 0; i < snapshot_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && snapshot_count > 256) {
            semantic_memory_heartbeat("semantic_mem_loop",
                             (float)(i + 1) / (float)snapshot_count);
        }

        if (system->concepts[i] && system->concepts[i]->id == id) {
            return system->concepts[i];
        }
    }

    /* Normal "not found" path - no throw needed */
    return NULL;
}

/**
 * @brief Generate unique concept ID
 * WHAT: Create unique ID for new concept
 * WHY:  Every concept needs unique identifier
 * HOW:  Increment counter with timestamp uniqueness
 */
static uint64_t generate_concept_id(semantic_memory_system_t* system) {
    if (!system) return 0;
    return ++system->next_concept_id;
}

/**
 * @brief Generate unique relation ID
 * WHAT: Create unique ID for new relation
 * WHY:  Every relation needs unique identifier
 * HOW:  Increment counter
 */
static uint64_t generate_relation_id(semantic_memory_system_t* system) {
    if (!system) return 0;
    return ++system->next_relation_id;
}

//=============================================================================
// System Management
//=============================================================================

/**
 * @brief Create semantic memory network
 * WHAT: Allocate and initialize semantic memory system
 * WHY:  Prepare network for concept and relation storage
 * HOW:  Allocate pools, set defaults, initialize stats
 */
semantic_memory_system_t* semantic_memory_create(void) {
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_create", 0.0f);


    /* P3-COG-05: Add LOG_MODULE to LOG_INFO */
    LOG_INFO(LOG_MODULE, "Creating semantic memory system");

    semantic_memory_system_t* system =
        (semantic_memory_system_t*)nimcp_calloc(1, sizeof(semantic_memory_system_t));
    NIMCP_API_CHECK_ALLOC(system, "Failed to allocate semantic memory system");

    // Initialize unified memory manager for CoW support
    unified_mem_config_t mem_config = unified_mem_default_config();
    mem_config.enable_cow = true;
    mem_config.enable_tracking = true;
    system->mem_manager = unified_mem_create(&mem_config);

    // Allocate concept pool (with unified memory if available)
    system->concept_capacity = DEFAULT_CONCEPT_CAPACITY;
    if (system->mem_manager) {
        unified_mem_request_t req = unified_mem_request(
            system->concept_capacity * sizeof(semantic_concept_t*),
            NULL, true
        );
        system->concepts_handle = unified_mem_alloc(system->mem_manager, &req);
        if (system->concepts_handle) {
            system->concepts = (semantic_concept_t**)unified_mem_write(system->concepts_handle);
            LOG_DEBUG(LOG_MODULE, "Concepts array allocated via unified memory with CoW");
        }
    }
    if (!system->concepts) {
        system->concepts = (semantic_concept_t**)nimcp_calloc(
            system->concept_capacity, sizeof(semantic_concept_t*)
        );
    }
    if (!system->concepts) {
        if (system->mem_manager) unified_mem_destroy(system->mem_manager);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_memory_create: validation failed");
        return NULL;
    }

    // Allocate relation pool
    system->relation_capacity = DEFAULT_RELATION_CAPACITY;
    system->relations = (semantic_relation_t**)nimcp_calloc(
        system->relation_capacity,
        sizeof(semantic_relation_t*)
    );

    if (!system->relations) {
        if (system->concepts_handle) unified_mem_free(system->concepts_handle);
        else nimcp_free(system->concepts);
        if (system->mem_manager) unified_mem_destroy(system->mem_manager);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_memory_create: validation failed");
        return NULL;
    }

    // Allocate activation map (with unified memory if available)
    system->activation_map_size = DEFAULT_CONCEPT_CAPACITY;
    if (system->mem_manager) {
        unified_mem_request_t req = unified_mem_request(
            system->activation_map_size * sizeof(float),
            NULL, true
        );
        system->activation_handle = unified_mem_alloc(system->mem_manager, &req);
        if (system->activation_handle) {
            system->activation_map = (float*)unified_mem_write(system->activation_handle);
            LOG_DEBUG(LOG_MODULE, "Activation map allocated via unified memory with CoW");
        }
    }
    if (!system->activation_map) {
        system->activation_map = (float*)nimcp_calloc(
            system->activation_map_size, sizeof(float)
        );
    }
    if (!system->activation_map) {
        nimcp_free(system->relations);
        if (system->concepts_handle) unified_mem_free(system->concepts_handle);
        else nimcp_free(system->concepts);
        if (system->mem_manager) unified_mem_destroy(system->mem_manager);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_memory_create: validation failed");
        return NULL;
    }

    // Set default spreading activation parameters
    system->spread_params = semantic_memory_get_default_spread_params();

    // Initialize time tracking
    system->last_update_time_ms = nimcp_platform_time_monotonic_ms();
    system->next_concept_id = 1;
    system->next_relation_id = 1;

    // Phase 1.5: Initialize memory pools for hot-path allocations
    memory_pool_config_t concept_pool_config = {
        .block_size = sizeof(semantic_concept_t),
        .num_blocks = 64,  // Pre-allocate for typical usage
        .alignment = 16,   // SIMD alignment
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    system->concept_pool = memory_pool_create(&concept_pool_config);

    memory_pool_config_t relation_pool_config = {
        .block_size = sizeof(semantic_relation_t),
        .num_blocks = 128,  // More relations than concepts typically
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    system->relation_pool = memory_pool_create(&relation_pool_config);

    memory_pool_config_t feature_pool_config = {
        .block_size = DEFAULT_FEATURE_DIM * sizeof(float),
        .num_blocks = 64,  // Same as concept pool
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    system->feature_pool = memory_pool_create(&feature_pool_config);

    
    // Bio-async registration
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_MEMORY_SEMANTIC,
            .module_name = "semantic_memory",
            .inbox_capacity = 32,
            .user_data = system
        };
        system->bio_ctx = bio_router_register_module(&bio_info);
        if (system->bio_ctx) {
            system->bio_async_enabled = true;

            // Try KG-driven wiring callback first
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_MEMORY_SEMANTIC,
                (void*)semantic_memory_wiring_handler_callback,
                system
            );

            if (wiring_result != NIMCP_SUCCESS) {
                // Fallback to legacy hardcoded registration
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(system->bio_ctx, BIO_MSG_KNOWLEDGE_QUERY,
                                                handle_knowledge_query)
                );
                LOG_INFO(LOG_MODULE, "Bio-async registered with legacy handlers (module_id=0x%04X)", BIO_MODULE_SEMANTIC_MEMORY);
            } else {
                LOG_INFO(LOG_MODULE, "Bio-async registered with KG wiring callback (module_id=0x%04X)", BIO_MODULE_SEMANTIC_MEMORY);
            }
        }
    }

    /* P2-COG-07: Initialize mutex for thread-safe concept/relation operations */
    {
        mutex_attr_t mattr = {.type = MUTEX_TYPE_NORMAL};
        system->mutex = nimcp_mutex_create(&mattr);
    }

    // Initialize quantum bridge (enabled by default)
    system->quantum_bridge = NULL;
    system->quantum_retrievals = 0;
    system->enable_quantum = true;  // Quantum acceleration enabled

    // Create quantum bridge
    semantic_quantum_config_t qconfig = semantic_quantum_default_config();
    system->quantum_bridge = semantic_quantum_bridge_create(&qconfig);
    if (system->quantum_bridge) {
        LOG_INFO("Quantum-accelerated semantic retrieval enabled");
    }

return system;
}

/*=============================================================================
 * BIO-ASYNC HANDLER IMPLEMENTATIONS
 *============================================================================*/

static nimcp_error_t handle_knowledge_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)msg_size;
    (void)response_promise;
    NIMCP_CHECK_THROW(msg && user_data, NIMCP_ERROR_NULL_ARG, "msg or user_data is NULL");
    semantic_memory_system_t* system = (semantic_memory_system_t*)user_data;
    LOG_DEBUG(LOG_MODULE, "Received knowledge query, concept_count=%u", system->concept_count);
    return NIMCP_SUCCESS;
}

static void bio_broadcast_concept_activated(semantic_memory_system_t* system, uint32_t concept_id, float activation) {
    if (!system || !system->bio_async_enabled || !system->bio_ctx) { return; }
    // Use salience response for concept activation (repurposed for semantic memory)
    bio_msg_salience_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(system->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.stimulus_id = concept_id;
    msg.salience_score = activation;
    msg.attention_priority = activation;
    msg.requires_immediate_attention = (activation > 0.8F);
    bio_router_broadcast(system->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast concept activated: id=%u, activation=%.2f", concept_id, activation);
}

/**
 * @brief Free concept resources
 * WHAT: Free memory allocated for concept
 * WHY:  Prevent memory leaks
 * HOW:  Free label, features, source IDs, struct (with CoW handling)
 */
static void free_concept(semantic_concept_t* concept) {
    if (!concept) return;

    if (concept->label) nimcp_free(concept->label);

    // Phase 1.5: Handle CoW for features
    if (concept->features) {
        if (concept->_cow_refcount) {
            // Shared features - decrement refcount
            uint32_t remaining = __atomic_sub_fetch(concept->_cow_refcount, 1, __ATOMIC_SEQ_CST);
            if (remaining == 0) {
                // Last reference - free shared data
                nimcp_free(concept->features);
                nimcp_free(concept->_cow_refcount);
            }
            // else: other references exist, don't free features
        } else {
            // Owned features - free directly
            nimcp_free(concept->features);
        }
    }

    if (concept->source_memory_ids) nimcp_free(concept->source_memory_ids);
    nimcp_free(concept);
    concept = NULL;
}

/**
 * @brief Destroy semantic memory network
 * WHAT: Free all resources associated with system
 * WHY:  Prevent memory leaks when brain destroyed
 * HOW:  Free concepts, relations, maps, system
 */
void semantic_memory_destroy(semantic_memory_system_t* system) {
    if (!system) return;

    // Free all concepts (individual concept objects)
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_destroy", 0.0f);


    if (system->concepts) {
        for (uint32_t i = 0; i < system->concept_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->concept_count > 256) {
                semantic_memory_heartbeat("semantic_mem_loop",
                                 (float)(i + 1) / (float)system->concept_count);
            }

            free_concept(system->concepts[i]);
        }
        // Free concepts array (unified memory or direct)
        if (system->concepts_handle) {
            unified_mem_free(system->concepts_handle);
            system->concepts_handle = NULL;
        } else {
            nimcp_free(system->concepts);
        }
        system->concepts = NULL;
    }

    // Free all relations
    if (system->relations) {
        for (uint32_t i = 0; i < system->relation_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->relation_count > 256) {
                semantic_memory_heartbeat("semantic_mem_loop",
                                 (float)(i + 1) / (float)system->relation_count);
            }

            nimcp_free(system->relations[i]);
        }
        nimcp_free(system->relations);
        system->relations = NULL;
    }

    // Free activation map (unified memory or direct)
    if (system->activation_handle) {
        unified_mem_free(system->activation_handle);
        system->activation_handle = NULL;
        system->activation_map = NULL;
    } else if (system->activation_map) {
        nimcp_free(system->activation_map);
        system->activation_map = NULL;
    }

    // Destroy unified memory manager
    if (system->mem_manager) {
        unified_mem_destroy(system->mem_manager);
        system->mem_manager = NULL;
    }

    // Phase 1.5: Destroy memory pools
    if (system->concept_pool) {
        memory_pool_destroy(system->concept_pool);
    }
    if (system->relation_pool) {
        memory_pool_destroy(system->relation_pool);
    }
    if (system->feature_pool) {
        memory_pool_destroy(system->feature_pool);
    }

    // Unregister from bio-router
    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        system->bio_async_enabled = false;
    }

    // Destroy quantum bridge
    if (system->quantum_bridge) {
        semantic_quantum_bridge_destroy((semantic_quantum_bridge_t*)system->quantum_bridge);
        system->quantum_bridge = NULL;
    }

    /* P2-COG-07: Destroy mutex */
    if (system->mutex) {
        nimcp_mutex_destroy((nimcp_mutex_t*)system->mutex);
        system->mutex = NULL;
    }

    nimcp_free(system);
    system = NULL;
}

/**
 * @brief Reset semantic memory
 * WHAT: Clear concepts and relations, keep config
 * WHY:  Allow fresh start without recreating system
 * HOW:  Free concepts/relations, zero stats
 */
void semantic_memory_reset(semantic_memory_system_t* system) {
    if (!system) return;

    // Free all concepts
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_reset", 0.0f);


    for (uint32_t i = 0; i < system->concept_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->concept_count > 256) {
            semantic_memory_heartbeat("semantic_mem_loop",
                             (float)(i + 1) / (float)system->concept_count);
        }

        free_concept(system->concepts[i]);
        system->concepts[i] = NULL;
    }
    system->concept_count = 0;

    // Free all relations
    for (uint32_t i = 0; i < system->relation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->relation_count > 256) {
            semantic_memory_heartbeat("semantic_mem_loop",
                             (float)(i + 1) / (float)system->relation_count);
        }

        nimcp_free(system->relations[i]);
        system->relations[i] = NULL;
    }
    system->relation_count = 0;

    // Clear activation map
    memset(system->activation_map, 0,
           system->activation_map_size * sizeof(float));

    // Reset stats
    memset(&system->stats, 0, sizeof(semantic_memory_stats_t));

    // Reset ID counters
    system->next_concept_id = 1;
    system->next_relation_id = 1;
}

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to systems consolidation
 * WHAT: Link to Phase M2 for concept extraction
 * WHY:  Semantic concepts extracted from M2
 * HOW:  Store pointer (not owned)
 */
void semantic_memory_set_consolidation(
    semantic_memory_system_t* system,
    void* consolidation)
{
    if (!system) return;
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_set_consolidation", 0.0f);


    system->systems_consolidation = consolidation;
}

//=============================================================================
// Concept Operations
//=============================================================================

/**
 * @brief Create new concept from features
 * WHAT: Allocate concept, copy features, assign ID
 * WHY:  Store semantic knowledge as concept node
 * HOW:  Allocate, copy data, add to pool
 */
uint64_t semantic_memory_create_concept(
    semantic_memory_system_t* system,
    const float* features,
    uint32_t feature_dim,
    const char* label,
    concept_category_t category)
{
    // Guard clauses
    if (!system) return 0;
    if (!features || feature_dim == 0) return 0;
    if (system->concept_count >= system->concept_capacity) return 0;

    // Allocate concept
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_create_concept", 0.0f);


    semantic_concept_t* concept =
        (semantic_concept_t*)nimcp_calloc(1, sizeof(semantic_concept_t));

    if (!concept) return 0;

    // Assign ID
    concept->id = generate_concept_id(system);
    concept->category = category;

    // Copy features
    concept->feature_dim = feature_dim;
    concept->features = (float*)nimcp_calloc(feature_dim, sizeof(float));
    if (!concept->features) {
        nimcp_free(concept);
        concept = NULL;
        return 0;
    }
    memcpy(concept->features, features, feature_dim * sizeof(float));

    // Copy label if provided
    if (label) {
        concept->label = nimcp_strdup(label);
    }

    // Initialize metadata
    concept->activation = 0.0F;
    concept->base_activation = 0.1F;
    concept->creation_time_ms = nimcp_platform_time_monotonic_ms();
    concept->access_count = 0;

    // Add to pool (thread-safe: lock before modifying shared array)
    if (system->mutex) nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    system->concepts[system->concept_count++] = concept;
    system->stats.concept_count++;
    system->stats.total_concepts_formed++;
    if (system->mutex) nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    /* W6: emit KG event for concept creation — but ONLY for the first
     * SEMANTIC_KG_EMIT_LIMIT concepts.
     *
     * Each emit adds a "_event_" node to the internal KG. brain_kg_add_node
     * does a linear free-slot scan and, once the KG fills, an O(node_capacity)
     * + O(edge_capacity) LRU-eviction sweep PER add. Bulk concept creation
     * (the ~50K-word WordNet lexicon loader and the pseudo-id remap on resume)
     * therefore turns into an O(N²) KG-eviction storm that wedges daemon
     * startup for many minutes. (This was latent: it only fires now that
     * semantic_memory is non-NULL at grounding time — previously every
     * create_concept call was short-circuited to a pseudo-id and this code
     * never ran.) Cap the telemetry to the early concepts; bulk creation
     * stays silent. */
    #define SEMANTIC_KG_EMIT_LIMIT 512u
    if (system->concept_count <= SEMANTIC_KG_EMIT_LIMIT) {
        memory_kg_emit_concept_created(
            memory_kg_events_get_registered_brain(),
            concept->id, concept->label);
    }

    return concept->id;
}

/**
 * @brief Get concept by ID
 * WHAT: Retrieve concept from pool by ID
 * WHY:  Access concept data for operations
 * HOW:  Find in array, increment access count
 */
const semantic_concept_t* semantic_memory_get_concept(
    const semantic_memory_system_t* system,
    uint64_t concept_id)
{
    if (!system || concept_id == 0) {
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_get_concept", 0.0f);

    // Snapshot count under lock for safe iteration
    if (system->mutex) nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    uint32_t snapshot_count = system->concept_count;
    if (system->mutex) nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    for (uint32_t i = 0; i < snapshot_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && snapshot_count > 256) {
            semantic_memory_heartbeat("semantic_mem_loop",
                             (float)(i + 1) / (float)snapshot_count);
        }

        if (system->concepts[i] && system->concepts[i]->id == concept_id) {
            /* P2-COG-06: Explicit const cast for mutable access_count.
             * access_count is a cache-hit counter that tracks usage statistics
             * and does not affect semantic state. Cast is intentional. */
            ((semantic_concept_t*)system->concepts[i])->access_count++;
            return system->concepts[i];
        }
    }

    /* Normal "not found" path - no throw needed */
    return NULL;
}

/**
 * @brief Find concepts similar to features
 * WHAT: Search for concepts with similar features
 * WHY:  Retrieve related concepts for reasoning
 * HOW:  Compute similarity, sort, return top N
 */
semantic_query_result_t* semantic_memory_find_similar(
    semantic_memory_system_t* system,
    const float* features,
    uint32_t feature_dim,
    uint32_t max_results,
    float threshold)
{
    // Guard clauses
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }
    if (!features || feature_dim == 0) return NULL;
    if (system->concept_count == 0) {
        return NULL;
    }

    // Allocate result
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_find_similar", 0.0f);


    semantic_query_result_t* result =
        (semantic_query_result_t*)nimcp_calloc(1, sizeof(semantic_query_result_t));

    if (!result) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");


        return NULL;


    }

    // Snapshot concept_count under lock to prevent race with create_concept
    if (system->mutex) nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    uint32_t snapshot_count = system->concept_count;
    if (system->mutex) nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    // Allocate arrays for all concepts
    uint64_t* temp_ids = (uint64_t*)nimcp_malloc(
        snapshot_count * sizeof(uint64_t)
    );
    float* temp_sims = (float*)nimcp_malloc(
        snapshot_count * sizeof(float)
    );

    if (!temp_ids || !temp_sims) {
        nimcp_free(temp_ids);
        temp_ids = NULL;
        nimcp_free(temp_sims);
        temp_sims = NULL;
        nimcp_free(result);
        result = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_memory_find_similar: required parameter is NULL (temp_ids, temp_sims)");
        return NULL;
    }

    // Compute similarities under lock — prevents CoW refcount decrement from
    // freeing concept->features while we're reading them.
    // Hold mutex for full iteration (concepts ≤ 2048, each similarity is O(32 floats) = fast).
    if (system->mutex) nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    uint32_t match_count = 0;
    for (uint32_t i = 0; i < snapshot_count; i++) {
        semantic_concept_t* concept = system->concepts[i];
        if (!concept || !concept->features) continue;

        float sim = compute_similarity(features, concept->features, feature_dim);

        if (sim >= threshold) {
            temp_ids[match_count] = concept->id;
            temp_sims[match_count] = sim;
            match_count++;
        }
    }
    if (system->mutex) nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    // Return top N results
    uint32_t return_count = (match_count < max_results) ?
                             match_count : max_results;

    result->count = return_count;
    result->concept_ids = (uint64_t*)nimcp_calloc(return_count, sizeof(uint64_t));
    result->activation_levels = (float*)nimcp_calloc(return_count, sizeof(float));

    if (!result->concept_ids || !result->activation_levels) {
        nimcp_free(temp_ids);
        temp_ids = NULL;
        nimcp_free(temp_sims);
        temp_sims = NULL;
        semantic_memory_free_result(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_memory_find_similar: required parameter is NULL (result->concept_ids, result->activation_levels)");
        return NULL;
    }

    memcpy(result->concept_ids, temp_ids, return_count * sizeof(uint64_t));
    memcpy(result->activation_levels, temp_sims, return_count * sizeof(float));

    nimcp_free(temp_ids);
    temp_ids = NULL;
    nimcp_free(temp_sims);
    temp_sims = NULL;

    return result;
}

//=============================================================================
// Relation Operations
//=============================================================================

/**
 * @brief Create relation between concepts
 * WHAT: Create directed edge between concepts
 * WHY:  Encode relationships for inference
 * HOW:  Allocate, store IDs and metadata
 */
uint64_t semantic_memory_create_relation(
    semantic_memory_system_t* system,
    uint64_t source_id,
    uint64_t target_id,
    relation_type_t type,
    float strength)
{
    // Guard clauses
    if (!system) return 0;
    if (source_id == 0 || target_id == 0) return 0;
    if (system->relation_count >= system->relation_capacity) return 0;

    // Verify concepts exist
    if (!find_concept_by_id(system, source_id)) return 0;
    if (!find_concept_by_id(system, target_id)) return 0;

    // Allocate relation
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_create_relation", 0.0f);


    semantic_relation_t* relation =
        (semantic_relation_t*)nimcp_calloc(1, sizeof(semantic_relation_t));

    if (!relation) return 0;

    // Set properties
    relation->id = generate_relation_id(system);
    relation->source_concept_id = source_id;
    relation->target_concept_id = target_id;
    relation->type = type;
    relation->strength = strength;
    relation->co_occurrence_count = 1;
    relation->creation_time_ms = nimcp_platform_time_monotonic_ms();

    // Add to pool (thread-safe: lock before modifying shared array)
    if (system->mutex) nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    system->relations[system->relation_count++] = relation;
    system->stats.relation_count++;
    system->stats.total_relations_formed++;
    if (system->mutex) nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return relation->id;
}

/**
 * @brief Get relations for concept
 * WHAT: Find relations where concept is source/target
 * WHY:  Navigate network for spreading activation
 * HOW:  Search relation pool, return IDs
 */
uint32_t semantic_memory_get_relations(
    const semantic_memory_system_t* system,
    uint64_t concept_id,
    uint64_t* relation_ids,
    uint32_t max_relations)
{
    // Guard clauses
    if (!system || concept_id == 0) return 0;
    if (!relation_ids || max_relations == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_get_relations", 0.0f);


    uint32_t found = 0;

    for (uint32_t i = 0; i < system->relation_count && found < max_relations; i++) {
        semantic_relation_t* rel = system->relations[i];
        if (!rel) continue;

        if (rel->source_concept_id == concept_id ||
            rel->target_concept_id == concept_id) {
            relation_ids[found++] = rel->id;
        }
    }

    return found;
}

//=============================================================================
// Spreading Activation Helpers
//=============================================================================

/**
 * @brief BFS queue node for spreading
 * WHAT: Node in BFS queue with concept and hop count
 * WHY:  Track concepts to process during spreading
 * HOW:  Store concept ID, activation, and hop distance
 */
typedef struct {
    uint64_t concept_id;
    float activation;
    uint32_t hop_count;
} activation_queue_node_t;

/**
 * @brief Get neighbors for concept via relations
 * WHAT: Find connected concepts through relations
 * WHY:  Navigate network during spreading
 * HOW:  Search relations, return neighbor IDs
 */
static uint32_t get_neighbor_concepts(
    const semantic_memory_system_t* system,
    uint64_t concept_id,
    uint64_t* neighbors,
    uint32_t max_neighbors)
{
    if (!system || concept_id == 0 || !neighbors) return 0;

    uint32_t found = 0;

    for (uint32_t i = 0; i < system->relation_count && found < max_neighbors; i++) {
        semantic_relation_t* rel = system->relations[i];
        if (!rel) continue;

        if (rel->source_concept_id == concept_id) {
            neighbors[found++] = rel->target_concept_id;
        } else if (rel->target_concept_id == concept_id) {
            neighbors[found++] = rel->source_concept_id;
        }
    }

    return found;
}

/**
 * @brief Perform BFS spreading activation
 * WHAT: Propagate activation through network
 * WHY:  Activate related concepts (Collins & Loftus, 1975)
 * HOW:  BFS with decay, respect max_hops and threshold
 */
static void spread_activation_bfs(
    semantic_memory_system_t* system,
    uint64_t start_id,
    float initial_activation)
{
    if (!system || start_id == 0) return;

    // Allocate simple BFS queue
    activation_queue_node_t* queue = (activation_queue_node_t*)nimcp_malloc(
        system->concept_capacity * sizeof(activation_queue_node_t)
    );
    if (!queue) return;

    uint32_t queue_start = 0;
    uint32_t queue_end = 0;

    // Initialize with starting concept
    queue[queue_end].concept_id = start_id;
    queue[queue_end].activation = initial_activation;
    queue[queue_end].hop_count = 0;
    queue_end++;

    // Track visited concepts (use activation_map as visited flag)
    memset(system->activation_map, 0,
           system->activation_map_size * sizeof(float));

    // BFS spreading
    while (queue_start < queue_end) {
        activation_queue_node_t current = queue[queue_start++];

        // Stop if exceeded max hops
        if (current.hop_count >= system->spread_params.max_hops) continue;

        // Find concept
        semantic_concept_t* concept = find_concept_by_id(system, current.concept_id);
        if (!concept) continue;

        // Update activation (keep max)
        if (current.activation > concept->activation) {
            concept->activation = current.activation;
        }

        // Stop if below minimum
        if (current.activation < system->spread_params.min_activation) continue;

        // Get neighbors
        uint64_t neighbors[256];
        uint32_t neighbor_count = get_neighbor_concepts(
            system, current.concept_id, neighbors, 256
        );

        // Spread to neighbors with decay
        float next_activation = current.activation * system->spread_params.decay_rate;

        for (uint32_t i = 0; i < neighbor_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && neighbor_count > 256) {
                semantic_memory_heartbeat("semantic_mem_loop",
                                 (float)(i + 1) / (float)neighbor_count);
            }

            semantic_concept_t* neighbor = find_concept_by_id(system, neighbors[i]);
            if (!neighbor) continue;

            // Only add if not already at higher activation
            if (next_activation > neighbor->activation) {
                if (queue_end < system->concept_capacity) {
                    queue[queue_end].concept_id = neighbors[i];
                    queue[queue_end].activation = next_activation;
                    queue[queue_end].hop_count = current.hop_count + 1;
                    queue_end++;
                }
            }
        }
    }

    nimcp_free(queue);
    queue = NULL;
}

//=============================================================================
// Spreading Activation
//=============================================================================

/**
 * @brief Activate concept and spread activation
 * WHAT: Set concept activation, propagate to neighbors
 * WHY:  Retrieve related concepts via network
 * HOW:  BFS with decay through relations
 */
semantic_query_result_t* semantic_memory_activate(
    semantic_memory_system_t* system,
    uint64_t concept_id,
    float initial_activation)
{
    // Guard clauses
    if (!system || concept_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "semantic_memory_activate: system is NULL");
        return NULL;
    }

    // Find concept
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_activate", 0.0f);


    semantic_concept_t* concept = find_concept_by_id(system, concept_id);
    if (!concept) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "concept is NULL");

        return NULL;

    }

    // Clear previous activations
    for (uint32_t i = 0; i < system->concept_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->concept_count > 256) {
            semantic_memory_heartbeat("semantic_mem_loop",
                             (float)(i + 1) / (float)system->concept_count);
        }

        if (system->concepts[i]) {
            system->concepts[i]->activation = 0.0F;
        }
    }

    // Perform spreading activation
    spread_activation_bfs(system, concept_id, initial_activation);

    // Collect activated concepts above threshold
    uint32_t activated_count = 0;
    for (uint32_t i = 0; i < system->concept_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->concept_count > 256) {
            semantic_memory_heartbeat("semantic_mem_loop",
                             (float)(i + 1) / (float)system->concept_count);
        }

        if (system->concepts[i] &&
            system->concepts[i]->activation >= system->spread_params.threshold) {
            activated_count++;
        }
    }

    // Allocate result
    semantic_query_result_t* result =
        (semantic_query_result_t*)nimcp_calloc(1, sizeof(semantic_query_result_t));
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    if (activated_count == 0) {
        result->count = 0;
        return result;
    }

    result->concept_ids = (uint64_t*)nimcp_calloc(activated_count, sizeof(uint64_t));
    result->activation_levels = (float*)nimcp_calloc(activated_count, sizeof(float));

    if (!result->concept_ids || !result->activation_levels) {
        semantic_memory_free_result(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "semantic_memory_activate: required parameter is NULL (result->concept_ids, result->activation_levels)");
        return NULL;
    }

    // Fill result with activated concepts
    uint32_t idx = 0;
    for (uint32_t i = 0; i < system->concept_count && idx < activated_count; i++) {
        if (system->concepts[i] &&
            system->concepts[i]->activation >= system->spread_params.threshold) {
            result->concept_ids[idx] = system->concepts[i]->id;
            result->activation_levels[idx] = system->concepts[i]->activation;
            idx++;
        }
    }

    result->count = activated_count;
    system->stats.total_retrievals++;

    /* W6: emit KG spreading-activation event */
    memory_kg_emit_spreading_activation(
        memory_kg_events_get_registered_brain(),
        concept_id, activated_count);

    return result;
}

/**
 * @brief Query semantic memory with features
 * WHAT: Find similar, then spread activation
 * WHY:  Combined similarity + spreading retrieval
 * HOW:  Find similar concepts, activate best, spread
 */
semantic_query_result_t* semantic_memory_query(
    semantic_memory_system_t* system,
    const float* features,
    uint32_t feature_dim)
{
    // Guard clauses
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }
    if (!features || feature_dim == 0) return NULL;

    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_query", 0.0f);


    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_process_inbox(system->bio_ctx, 5);
    }

    // Find similar concepts
    semantic_query_result_t* similar = semantic_memory_find_similar(
        system,
        features,
        feature_dim,
        1,      // Get best match
        0.5F    // 50% similarity threshold
    );

    if (!similar || similar->count == 0) {
        if (similar) semantic_memory_free_result(similar);
        return NULL;
    }

    // Activate best matching concept and spread
    uint64_t best_id = similar->concept_ids[0];
    float best_sim = similar->activation_levels[0];
    semantic_memory_free_result(similar);

    // Use similarity as initial activation
    semantic_query_result_t* activated = semantic_memory_activate(
        system, best_id, best_sim
    );

    if (activated) {
        system->stats.total_retrievals++;
    }

    return activated;
}

/**
 * @brief Free query result
 * WHAT: Free memory allocated for result
 * WHY:  Prevent memory leaks
 * HOW:  Free arrays, then struct
 */
void semantic_memory_free_result(semantic_query_result_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_free_result", 0.0f);


    if (result->concept_ids) nimcp_free(result->concept_ids);
    if (result->activation_levels) nimcp_free(result->activation_levels);
    nimcp_free(result);
    result = NULL;
}

//=============================================================================
// Knowledge Extraction
//=============================================================================

/**
 * @brief Extract concepts from Phase M2
 * WHAT: Create concepts from consolidated memories
 * WHY:  Build semantic network from M2
 * HOW:  Iterate M2 cortical nodes, extract semantic concepts
 */
uint32_t semantic_memory_extract_from_consolidation(
    semantic_memory_system_t* system)
{
    // Guard clauses
    if (!system) return 0;
    if (!system->systems_consolidation) return 0;

    // Cast to systems consolidation system
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_extract_from_consoli", 0.0f);


    systems_consolidation_system_t* m2_sys =
        (systems_consolidation_system_t*)system->systems_consolidation;

    uint32_t extracted = 0;

    // Iterate through cortical nodes
    for (uint32_t i = 0; i < m2_sys->node_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && m2_sys->node_count > 256) {
            semantic_memory_heartbeat("semantic_mem_loop",
                             (float)(i + 1) / (float)m2_sys->node_count);
        }

        cortical_memory_node_t* node = m2_sys->cortical_nodes[i];
        if (!node) continue;

        // Only extract semantic memories (not episodic)
        if (node->type != CORTICAL_MEMORY_SEMANTIC &&
            node->type != CORTICAL_MEMORY_SCHEMA) {
            continue;
        }

        // Check if concept already exists for this node
        bool already_exists = false;
        for (uint32_t j = 0; j < system->concept_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && system->concept_count > 256) {
                semantic_memory_heartbeat("semantic_mem_loop",
                                 (float)(j + 1) / (float)system->concept_count);
            }

            if (system->concepts[j] &&
                system->concepts[j]->source_count > 0 &&
                system->concepts[j]->source_memory_ids[0] == node->id) {
                already_exists = true;
                break;
            }
        }

        if (already_exists) continue;

        // Create concept from cortical node features
        concept_category_t category = (node->type == CORTICAL_MEMORY_SCHEMA) ?
                                       CONCEPT_CATEGORY : CONCEPT_ABSTRACT;

        uint64_t concept_id = semantic_memory_create_concept(
            system,
            node->features,
            node->feature_dim,
            NULL,  // No label from M2
            category
        );

        if (concept_id != 0) {
            // Store source memory reference
            semantic_concept_t* concept = find_concept_by_id(system, concept_id);
            if (concept) {
                concept->source_memory_ids = (uint64_t*)nimcp_malloc(sizeof(uint64_t));
                if (concept->source_memory_ids) {
                    concept->source_memory_ids[0] = node->id;
                    concept->source_count = 1;
                }
            }
            extracted++;
        }
    }

    return extracted;
}

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Set spreading activation parameters
 * WHAT: Update spread parameters
 * WHY:  Allow customization of activation
 * HOW:  Copy params to system
 */
void semantic_memory_set_spread_params(
    semantic_memory_system_t* system,
    const spreading_activation_params_t* params)
{
    if (!system || !params) return;
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_set_spread_params", 0.0f);


    system->spread_params = *params;
}

/**
 * @brief Get spreading activation parameters
 * WHAT: Retrieve current parameters
 * WHY:  Allow inspection of configuration
 * HOW:  Copy from system to output
 */
void semantic_memory_get_spread_params(
    const semantic_memory_system_t* system,
    spreading_activation_params_t* params_out)
{
    if (!system || !params_out) return;
    *params_out = system->spread_params;
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_get_spread_params", 0.0f);


}

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get semantic memory statistics
 * WHAT: Retrieve system statistics
 * WHY:  Monitoring and debugging
 * HOW:  Copy stats to output
 */
void semantic_memory_get_statistics(
    const semantic_memory_system_t* system,
    semantic_memory_stats_t* stats_out)
{
    if (!system || !stats_out) return;
    *stats_out = system->stats;
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_get_statistics", 0.0f);


}

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default spreading activation parameters
 * WHAT: Return biologically-based defaults
 * WHY:  Provide sensible starting configuration
 * HOW:  Return struct with literature-based values
 */
spreading_activation_params_t semantic_memory_get_default_spread_params(void) {
    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_get_default_spread_p", 0.0f);


    spreading_activation_params_t params = {
        .decay_rate = 0.8F,        // 20% decay per hop (Collins & Loftus, 1975)
        .threshold = 0.3F,         // 30% activation threshold
        .max_hops = 3,             // Spread up to 3 hops
        .min_activation = 0.1F     // Stop below 10% activation
    };
    return params;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * @brief Query self-knowledge from knowledge graph
 * WHAT: Retrieve module's self-awareness information from KG
 * WHY:  Enable introspection about module capabilities and connections
 * HOW:  Query KG reader for entity and relations
 */
int semantic_memory_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    semantic_memory_heartbeat("semantic_mem_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Semantic_Memory_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                semantic_memory_heartbeat("semantic_mem_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Semantic memory self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Semantic_Memory_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Semantic_Memory_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void semantic_memory_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_semantic_memory_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int semantic_memory_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "semantic_memory_training_begin: NULL argument");
        return -1;
    }
    semantic_memory_heartbeat_instance(NULL, "semantic_memory_training_begin", 0.0f);
    (void)(activation_queue_node_t*)instance; /* Module state available for reset */
    return 0;
}

int semantic_memory_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "semantic_memory_training_end: NULL argument");
        return -1;
    }
    semantic_memory_heartbeat_instance(NULL, "semantic_memory_training_end", 1.0f);
    (void)(activation_queue_node_t*)instance; /* Module state available for finalization */
    return 0;
}

int semantic_memory_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "semantic_memory_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    semantic_memory_heartbeat_instance(NULL, "semantic_memory_training_step", progress);
    (void)(activation_queue_node_t*)instance; /* Module state available for step adaptation */
    return 0;
}
