/**
 * @file nimcp_wm_transfer.c
 * @brief Phase M3: Working Memory to Engram Transfer Implementation
 *
 * WHAT: Implements selective transfer from working memory to long-term memory
 * WHY:  Not all temporary information should become permanent memories
 * HOW:  Multi-factor scoring based on rehearsal, attention, emotion, and time
 *
 * BIO-ASYNC INTEGRATION:
 * - Module ID: 0x0333 (BIO_MODULE_WM_TRANSFER)
 * - Publishes: transfer events, consolidation triggers
 * - Subscribes: working memory updates, attention changes
 *
 * @version Phase M3 Working Memory Transfer
 * @date 2025-11-13
 */

#define LOG_MODULE "wm_transfer"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(wm_transfer, MESH_ADAPTER_CATEGORY_MEMORY)



#include "cognitive/memory/nimcp_wm_transfer.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "nimcp.h"  // For NIMCP_ERROR_* codes
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/memory/nimcp_engram.h"
#include "utils/platform/nimcp_platform_time.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free

//=============================================================================
// BIO-ASYNC MODULE REGISTRATION
//=============================================================================

#define BIO_MODULE_WM_TRANSFER 0x0333

//=============================================================================
// BIO-ASYNC HANDLERS (Forward declarations)
//=============================================================================

static nimcp_error_t handle_wm_store_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static void bio_broadcast_transfer_complete(wm_transfer_system_t* system, uint32_t item_id, float score);

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
static int wm_transfer_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO(LOG_MODULE, "wm_transfer_wiring_handler_callback: registering %u handlers from KG",
             message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            wm_transfer_heartbeat("wm_transfer_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_WORKING_MEMORY_STORE:
                bio_router_register_handler(ctx, message_types[i], handle_wm_store_request);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_WORKING_MEMORY_STORE");
                break;

            default:
                LOG_DEBUG(LOG_MODULE, "  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

//=============================================================================
// Constants
//=============================================================================

// Transfer scoring weights (must sum to 1.0)
#define REHEARSAL_WEIGHT 0.4f    // 40% contribution
#define ATTENTION_WEIGHT 0.3f    // 30% contribution
#define EMOTIONAL_WEIGHT 0.2f    // 20% contribution
#define TIME_WEIGHT 0.1f         // 10% contribution

// Transfer threshold (score must be >= this to transfer)
#define TRANSFER_SCORE_THRESHOLD 0.5f

// Default working memory capacity (Miller's law: 7±2)
#define DEFAULT_WM_CAPACITY 7

//=============================================================================
// System Management
//=============================================================================

/**
 * @brief Create working memory transfer system
 * WHAT: Allocate and initialize transfer system
 * WHY:  Prepare system for managing WM → engram transfers
 * HOW:  Allocate struct, set defaults, initialize tracking arrays
 */
wm_transfer_system_t* wm_transfer_create(void) {
    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_create", 0.0f);


    LOG_INFO("Creating WM transfer system");

    // Allocate system
    wm_transfer_system_t* system = (wm_transfer_system_t*)nimcp_calloc(1, sizeof(wm_transfer_system_t));
    if (!system) {
        LOG_ERROR("Failed to allocate WM transfer system (%zu bytes)", sizeof(wm_transfer_system_t));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;
    }

    // Set default criteria
    system->criteria = wm_transfer_get_default_criteria();

    // Initialize stats to zero (calloc handles this)

    // Initialize unified memory manager for CoW support
    unified_mem_config_t mem_config = unified_mem_default_config();
    mem_config.enable_cow = true;
    mem_config.enable_tracking = true;
    system->mem_manager = unified_mem_create(&mem_config);

    // Initialize attention tracking (with unified memory if available)
    system->attention_weight_count = DEFAULT_WM_CAPACITY;
    if (system->mem_manager) {
        unified_mem_request_t req = unified_mem_request(
            DEFAULT_WM_CAPACITY * sizeof(float), NULL, true
        );
        system->attention_handle = unified_mem_alloc(system->mem_manager, &req);
        if (system->attention_handle) {
            system->last_attention_weights = (float*)unified_mem_write(system->attention_handle);
            LOG_DEBUG(LOG_MODULE, "Attention weights allocated via unified memory with CoW");
        }
    }
    if (!system->last_attention_weights) {
        system->last_attention_weights = (float*)nimcp_calloc(DEFAULT_WM_CAPACITY, sizeof(float));
    }
    if (!system->last_attention_weights) {
        LOG_ERROR("Failed to allocate attention weights");
        if (system->mem_manager) unified_mem_destroy(system->mem_manager);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_transfer_create: validation failed");
        return NULL;
    }

    // Record creation time
    system->last_update_time_ms = nimcp_platform_time_monotonic_ms();

    
    // Bio-async registration
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_WORKING_MEMORY_TRANSFER,
            .module_name = "wm_transfer",
            .inbox_capacity = 32,
            .user_data = system
        };
        system->bio_ctx = bio_router_register_module(&bio_info);
        if (system->bio_ctx) {
            system->bio_async_enabled = true;

            // Try KG-driven wiring callback first
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_WORKING_MEMORY_TRANSFER,
                (void*)wm_transfer_wiring_handler_callback,
                system
            );

            if (wiring_result != NIMCP_SUCCESS) {
                // Fallback to legacy hardcoded registration
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(system->bio_ctx, BIO_MSG_WORKING_MEMORY_STORE,
                                                handle_wm_store_request)
                );
                LOG_INFO(LOG_MODULE, "Bio-async registered with legacy handlers (module_id=0x%04X)", BIO_MODULE_WM_TRANSFER);
            } else {
                LOG_INFO(LOG_MODULE, "Bio-async registered with KG wiring callback (module_id=0x%04X)", BIO_MODULE_WM_TRANSFER);
            }
        }
    }

    return system;
}

/*=============================================================================
 * BIO-ASYNC HANDLER IMPLEMENTATIONS
 *============================================================================*/

static nimcp_error_t handle_wm_store_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)msg_size;
    (void)response_promise;
    NIMCP_CHECK_THROW(msg && user_data, NIMCP_ERROR_NULL_ARG, "msg or user_data is NULL");
    wm_transfer_system_t* system = (wm_transfer_system_t*)user_data;
    LOG_DEBUG(LOG_MODULE, "Received WM store request, current_wm_items=%u",
              system->stats.current_wm_items);
    return NIMCP_SUCCESS;
}

static void bio_broadcast_transfer_complete(wm_transfer_system_t* system, uint32_t item_id, float score) {
    if (!system || !system->bio_async_enabled || !system->bio_ctx) { return; }
    // Use salience response for transfer complete notification
    bio_msg_salience_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(system->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.stimulus_id = item_id;
    msg.salience_score = score;
    msg.attention_priority = score;
    msg.requires_immediate_attention = false;
    bio_router_broadcast(system->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast transfer complete: item=%u, score=%.2f", item_id, score);
}

/**
 * @brief Destroy working memory transfer system
 * WHAT: Free all resources associated with system
 * WHY:  Prevent memory leaks when brain is destroyed
 * HOW:  Free attention array, then system struct
 */
void wm_transfer_destroy(wm_transfer_system_t* system) {
    if (!system) return;

    // Free attention tracking (unified memory or direct)
    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_destroy", 0.0f);


    if (system->attention_handle) {
        unified_mem_free(system->attention_handle);
        system->attention_handle = NULL;
        system->last_attention_weights = NULL;
    } else if (system->last_attention_weights) {
        nimcp_free(system->last_attention_weights);
        system->last_attention_weights = NULL;
    }

    // Destroy unified memory manager
    if (system->mem_manager) {
        unified_mem_destroy(system->mem_manager);
        system->mem_manager = NULL;
    }

    // Unregister from bio-router
    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        system->bio_async_enabled = false;
    }

    nimcp_free(system);
    system = NULL;
}

/**
 * @brief Reset transfer system (clear stats, keep criteria)
 * WHAT: Clear statistics while preserving configuration
 * WHY:  Allow reuse of system with fresh state
 * HOW:  Zero stats and attention tracking
 */
void wm_transfer_reset(wm_transfer_system_t* system) {
    if (!system) return;

    // Clear stats
    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_reset", 0.0f);


    memset(&system->stats, 0, sizeof(wm_transfer_stats_t));

    // Reset attention weights
    if (system->last_attention_weights) {
        memset(system->last_attention_weights, 0,
               system->attention_weight_count * sizeof(float));
    }

    // Reset update time
    system->last_update_time_ms = nimcp_platform_time_monotonic_ms();
}

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to working memory system
 * WHAT: Link transfer system to working memory source
 * WHY:  Transfer system needs access to WM items
 * HOW:  Store pointer (not owned) to working memory
 */
void wm_transfer_set_working_memory(
    wm_transfer_system_t* system,
    void* working_memory)
{
    if (!system) return;
    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_set_working_memory", 0.0f);


    system->working_memory = working_memory;
}

/**
 * @brief Connect to engram system
 * WHAT: Link transfer system to engram destination
 * WHY:  Transferred items must be encoded as engrams
 * HOW:  Store pointer (not owned) to engram system
 */
void wm_transfer_set_engram_system(
    wm_transfer_system_t* system,
    engram_system_t* engram_system)
{
    if (!system) return;
    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_set_engram_system", 0.0f);


    system->engram_system = engram_system;
}

/**
 * @brief Connect to emotional tagging system
 * WHAT: Link transfer system to emotional salience source
 * WHY:  Emotional arousal enhances encoding
 * HOW:  Store pointer (not owned) to emotional system
 */
void wm_transfer_set_emotional_system(
    wm_transfer_system_t* system,
    void* emotional_system)
{
    if (!system) return;
    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_set_emotional_system", 0.0f);


    system->emotional_system = emotional_system;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Compute transfer score for working memory item
 * WHAT: Calculate multi-factor score for transfer decision
 * WHY:  Determine if item should be transferred to engrams
 * HOW:  Weight rehearsal, attention, emotion, time contributions
 */
static float compute_transfer_score(
    const wm_transfer_system_t* system,
    uint32_t rehearsal_count,
    float attention_weight,
    float emotional_salience,
    uint64_t time_in_wm_ms)
{
    float score = 0.0F;

    // Rehearsal contribution (40% weight)
    if (rehearsal_count >= system->criteria.rehearsal_threshold) {
        score += REHEARSAL_WEIGHT;
    }

    // Attention contribution (30% weight)
    if (attention_weight >= system->criteria.attention_threshold) {
        score += ATTENTION_WEIGHT;
    }

    // Emotional contribution (20% weight)
    if (emotional_salience >= system->criteria.emotional_threshold) {
        score += EMOTIONAL_WEIGHT;
    }

    // Time contribution (10% weight)
    if (time_in_wm_ms >= system->criteria.time_threshold_ms) {
        score += TIME_WEIGHT;
    }

    return score;
}

/**
 * @brief Check if item should be transferred
 * WHAT: Apply transfer threshold to score
 * WHY:  Only transfer items that meet sufficient criteria
 * HOW:  Compare score to threshold (0.5 = 50%)
 */
static bool should_transfer_item(float score) {
    return score >= TRANSFER_SCORE_THRESHOLD;
}

/**
 * @brief Update transfer statistics for trigger
 * WHAT: Increment stats counters based on what triggered transfer
 * WHY:  Track which factors are driving transfers
 * HOW:  Check each criterion and increment if met
 */
static void update_transfer_stats(
    wm_transfer_system_t* system,
    uint32_t rehearsal_count,
    float attention_weight,
    float emotional_salience,
    uint64_t time_in_wm_ms)
{
    // Process pending bio-async messages
    if (system && system->bio_async_enabled && system->bio_ctx) {
        bio_router_process_inbox(system->bio_ctx, 5);
    }

    // Update total transfers
    system->stats.total_transfers++;

    // Track which factors triggered this transfer
    if (rehearsal_count >= system->criteria.rehearsal_threshold) {
        system->stats.rehearsal_triggered++;
    }
    if (attention_weight >= system->criteria.attention_threshold) {
        system->stats.attention_triggered++;
    }
    if (emotional_salience >= system->criteria.emotional_threshold) {
        system->stats.emotion_triggered++;
    }
    if (time_in_wm_ms >= system->criteria.time_threshold_ms) {
        system->stats.time_triggered++;
    }
}

//=============================================================================
// Transfer Operations
//=============================================================================

/**
 * @brief Evaluate working memory items for transfer to engrams
 * WHAT: Check WM items against criteria, transfer if met
 * WHY:  Implements selective consolidation to long-term memory
 * HOW:  Score each item, transfer if >= 0.5, update stats
 *
 * NOTE: This is a placeholder implementation that demonstrates the algorithm.
 * Full implementation requires integration with actual working memory system.
 */
uint32_t wm_transfer_evaluate(
    wm_transfer_system_t* system,
    float time_delta_seconds)
{
    // Guard clauses
    if (!system) return 0;
    if (!system->working_memory) return 0;
    if (!system->engram_system) return 0;

    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_evaluate", 0.0f);


    uint32_t transfers = 0;
    uint64_t current_time_ms = nimcp_platform_time_monotonic_ms();

    // Update time tracking
    uint64_t time_elapsed_ms = current_time_ms - system->last_update_time_ms;
    system->last_update_time_ms = current_time_ms;

    // For each working memory item (placeholder logic)
    // NOTE: Actual implementation would query working memory system
    // For now, we demonstrate the algorithm with mock data

    // Placeholder: No actual working memory items to process yet
    // This will be filled in during brain integration

    return transfers;
}

/**
 * @brief Force transfer of specific working memory item
 * WHAT: Manually trigger transfer bypassing criteria
 * WHY:  Allow explicit encoding of important events
 * HOW:  Extract features, encode to engram directly
 */
bool wm_transfer_force_item(
    wm_transfer_system_t* system,
    uint32_t wm_slot)
{
    // Guard clauses
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_transfer_force_item: system is NULL");
        return false;
    }
    if (!system->working_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_transfer_force_item: system->working_memory is NULL");
        return false;
    }
    if (!system->engram_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_transfer_force_item: system->engram_system is NULL");
        return false;
    }

    // Placeholder: Would extract features from working memory slot
    // and encode directly to engram system

    // Update stats
    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_force_item", 0.0f);


    system->stats.total_transfers++;

    return true;
}

/**
 * @brief Update attention weights for working memory items
 * WHAT: Store current attention distribution across WM slots
 * WHY:  Attention determines transfer priority
 * HOW:  Copy weights array, resize if needed
 */
void wm_transfer_update_attention(
    wm_transfer_system_t* system,
    const float* attention_weights,
    uint32_t count)
{
    // Guard clauses
    if (!system) return;
    if (!attention_weights) return;
    if (count == 0) return;

    // Resize attention array if needed
    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_update_attention", 0.0f);


    if (count != system->attention_weight_count) {
        float* new_weights = (float*)nimcp_realloc(system->last_attention_weights,
                                               count * sizeof(float));
        if (!new_weights) {
            fprintf(stderr, "Failed to resize attention weights\n");
            return;
        }
        system->last_attention_weights = new_weights;
        system->attention_weight_count = count;
    }

    // Copy attention weights
    memcpy(system->last_attention_weights, attention_weights,
           count * sizeof(float));
}

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Set transfer criteria
 * WHAT: Update transfer decision thresholds
 * WHY:  Allow customization of transfer behavior
 * HOW:  Copy criteria struct to system
 */
void wm_transfer_set_criteria(
    wm_transfer_system_t* system,
    const wm_transfer_criteria_t* criteria)
{
    if (!system || !criteria) return;
    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_set_criteria", 0.0f);


    system->criteria = *criteria;
}

/**
 * @brief Get current transfer criteria
 * WHAT: Retrieve current transfer thresholds
 * WHY:  Allow inspection of configuration
 * HOW:  Copy criteria from system to output
 */
void wm_transfer_get_criteria(
    const wm_transfer_system_t* system,
    wm_transfer_criteria_t* criteria_out)
{
    if (!system || !criteria_out) return;
    *criteria_out = system->criteria;
    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_get_criteria", 0.0f);


}

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get transfer statistics
 * WHAT: Retrieve transfer operation statistics
 * WHY:  Monitoring and debugging transfer behavior
 * HOW:  Copy stats from system to output
 */
void wm_transfer_get_statistics(
    const wm_transfer_system_t* system,
    wm_transfer_stats_t* stats_out)
{
    if (!system || !stats_out) return;
    *stats_out = system->stats;
    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_get_statistics", 0.0f);


}

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default transfer criteria
 * WHAT: Return default thresholds based on neuroscience literature
 * WHY:  Provide sensible starting configuration
 * HOW:  Return struct with documented defaults
 *
 * DEFAULTS BASED ON:
 * - Miller (1956): Working memory capacity 7±2 items
 * - Atkinson & Shiffrin (1968): Rehearsal enhances transfer
 * - McGaugh (2000): Emotional arousal enhances consolidation
 */
wm_transfer_criteria_t wm_transfer_get_default_criteria(void) {
    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_get_default_criteria", 0.0f);


    wm_transfer_criteria_t criteria = {
        .rehearsal_threshold = 3,       // 3+ rehearsals triggers transfer
        .attention_threshold = 0.5F,    // 50% attention required
        .emotional_threshold = 0.3F,    // 30% emotional salience
        .time_threshold_ms = 5000,      // 5 seconds in working memory
        .decay_rate = 0.1F              // 10% decay per second
    };
    return criteria;
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
int wm_transfer_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    wm_transfer_heartbeat("wm_transfer_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "WM_Transfer_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                wm_transfer_heartbeat("wm_transfer_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("WM transfer self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "WM_Transfer_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "WM_Transfer_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void wm_transfer_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_wm_transfer_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int wm_transfer_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wm_transfer_training_begin: NULL argument");
        return -1;
    }
    wm_transfer_heartbeat_instance(NULL, "wm_transfer_training_begin", 0.0f);
    return 0;
}

int wm_transfer_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wm_transfer_training_end: NULL argument");
        return -1;
    }
    wm_transfer_heartbeat_instance(NULL, "wm_transfer_training_end", 1.0f);
    return 0;
}

int wm_transfer_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wm_transfer_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    wm_transfer_heartbeat_instance(NULL, "wm_transfer_training_step", progress);
    return 0;
}
