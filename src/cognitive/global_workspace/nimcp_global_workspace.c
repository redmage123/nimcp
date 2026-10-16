/**
 * @file nimcp_global_workspace.c
 * @brief Implementation of Global Workspace Theory for conscious access
 *
 * IMPLEMENTATION NOTES:
 * - Winner-take-all competition via max(signal_strengths)
 * - Circular buffer for broadcast history
 * - Competition decay via exponential forgetting
 * - Refractory period enforced via timestamp checking
 * - Statistics accumulated atomically (no threading, single brain)
 *
 * PERFORMANCE:
 * - Competition: O(N) where N = competitors (typically <10)
 * - Broadcast read: O(D) where D = content dim (memcpy)
 * - History: O(1) append, O(H) retrieval
 *
 * MEMORY:
 * - Workspace: ~300 bytes base
 * - Content: capacity_dim × sizeof(float) × 2 (current + temp)
 * - History: capacity_dim × sizeof(float) × history_depth
 * - Competitors: ~100 bytes × MAX_COMPETITORS
 * Total: ~50KB typical (256 dim, 10 history, 32 competitors)
 *
 * @author NIMCP Development Team - Part J
 * @date 2025-11-11
 */

#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/executive/nimcp_w9kg_events.h"  // W9-kg: KG event + read helpers

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <assert.h>

// Bio-async integration
#include "nimcp.h"  // For error codes
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "async/nimcp_bio_router.h"

// SNN and Plasticity bridge integration
#include "cognitive/global_workspace/nimcp_gw_snn_bridge.h"
#include "cognitive/global_workspace/nimcp_gw_plasticity_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "global_workspace"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(global_workspace, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Global workspace implementation (opaque)
 *
 * WHAT: Internal representation of workspace
 * WHY:  Encapsulation - users see only opaque pointer
 * HOW:  Struct contains all state and buffers
 */
struct global_workspace_struct {
    // Configuration (immutable after creation)
    global_workspace_config_t config;

    // Thread safety mutex for public API operations
    nimcp_platform_mutex_t mutex;           /**< Protects all mutable state */

    // Current broadcast state
    workspace_broadcast_t current_broadcast;
    float* broadcast_content;            /**< Content buffer [capacity_dim] */

    // Competition pool
    competitor_entry_t competitors[GLOBAL_WORKSPACE_MAX_COMPETITORS];
    uint32_t num_active_competitors;

    // Subscribers
    cognitive_module_t subscribers[GLOBAL_WORKSPACE_MAX_SUBSCRIBERS];
    uint32_t num_subscribers;

    // Broadcast history (circular buffer)
    workspace_broadcast_t* history;      /**< [history_depth] */
    float** history_content;             /**< [history_depth][capacity_dim] */
    uint32_t history_head;               /**< Circular buffer index */
    uint32_t history_count;              /**< How many in history */

    // Statistics
    workspace_statistics_t stats;

    // Timing state
    uint64_t last_broadcast_time_ms;
    uint64_t pool_activation_time_ms;  /**< When pool went from empty to non-empty */
    uint32_t next_broadcast_id;

    // Round-robin state (for ROUND_ROBIN strategy)
    uint32_t last_winner_idx;

    // Phase 1.5: Memory pools for hot-path allocations
    memory_pool_t broadcast_content_pool;   /**< Pool for broadcast content buffers */
    memory_pool_t history_content_pool;     /**< Pool for history content buffers */

    // Bio-async integration
    bio_module_context_t bio_ctx;           /**< Bio-async module context */
    bool bio_async_enabled;                 /**< Bio-async registration status */

    // SNN and Plasticity bridge integration
    gw_snn_bridge_t* snn_bridge;            /**< SNN bridge for spike-based processing */
    gw_plasticity_bridge_t* plasticity_bridge; /**< Plasticity bridge for learning */
    bool bridges_enabled;                   /**< Bridge integration status */
};

//=============================================================================
// BIO-ASYNC MESSAGE HANDLERS
//=============================================================================

// Forward declaration for handler function
static nimcp_error_t handle_attention_shift(const void*, size_t, nimcp_bio_promise_t, void*);


// Forward declarations for static functions (SRP split)
static int global_workspace_wiring_handler_callback( bio_module_context_t ctx, const bio_message_type_t* message_types, uint32_t message_count, void* user_data);
static void bio_broadcast_workspace_event(struct global_workspace_struct* ws, uint32_t broadcast_id, float strength);
static uint64_t get_time_ms(void);

//=============================================================================
// SRP #include-based split: Implementation parts
// These files contain the actual function implementations.
// DO NOT compile them separately - they are included here.
//=============================================================================

#include "nimcp_global_workspace_part_helpers.c"
#include "nimcp_global_workspace_part_lifecycle.c"
#include "nimcp_global_workspace_part_core.c"
#include "nimcp_global_workspace_part_accessors.c"
#include "nimcp_global_workspace_part_io.c"
#include "nimcp_global_workspace_part_processing.c"