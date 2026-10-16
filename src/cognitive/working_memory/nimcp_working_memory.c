/**
 * @file nimcp_working_memory.c
 * @brief Working memory implementation with temporal decay and attention refresh
 *
 * WHAT: Miller's 7±2 working memory buffer with salience-based eviction
 * WHY:  Maintain active representations for reasoning, planning, and decision-making
 * HOW:  Dynamic buffer with exponential decay, attention refresh, and priority eviction
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex (PFC) maintains ~7 items in active state
 * - Exponential decay without rehearsal (τ ≈ 1-2 seconds)
 * - Attention refresh prevents decay (frontal-parietal networks)
 * - Salience determines eviction priority (thalamic gating)
 *
 * BIO-ASYNC INTEGRATION:
 * - Module ID: 0x0334 (BIO_MODULE_WORKING_MEMORY)
 * - Publishes: item additions, evictions, refreshes
 * - Subscribes: attention updates, decay triggers
 *
 * PHASE: 10.2 (Working Memory)
 * DEPENDENCIES: None (standalone module)
 * TRAINING_IMPACT: None (inference-only, no weight modification)
 *
 * @author Claude Code
 * @date 2025-11
 */

#define LOG_MODULE "working_memory"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_dimension_constants.h"

BRIDGE_BOILERPLATE(working_memory, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include "cognitive/nimcp_working_memory.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "plasticity/nimcp_second_messengers.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"  // Global Workspace integration
#include "cognitive/immune/nimcp_brain_immune.h"  // Brain immune integration (cytokine enums)
#include "cognitive/nimcp_sleep_wake.h"  // Sleep state integration
#include "cognitive/working_memory/nimcp_working_memory_sleep_bridge.h"  // Sleep bridge for modulation
#include "core/brain/nimcp_brain_kg_helpers.h"  // KG self-awareness integration
#include "cognitive/executive/nimcp_w9kg_events.h"  // W9-kg: KG event + read helpers
#define NIMCP_WORKING_MEMORY_QUANTUM_BRIDGE_IMPLEMENTATION
#include "cognitive/memory/nimcp_working_memory_quantum_bridge.h"  // Quantum retrieval bridge
#include "cognitive/memory/nimcp_working_memory_snn_bridge.h"      // SNN bridge
#include "cognitive/memory/nimcp_working_memory_plasticity_bridge.h"  // Plasticity bridge

#include "nimcp.h"  // For error codes
#include "utils/error/nimcp_error_codes.h"  // For NIMCP_CHECK_NULL macros
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"  // Thread safety
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// BIO-ASYNC MODULE REGISTRATION
//=============================================================================

#define BIO_MODULE_WORKING_MEMORY 0x0334

// ============================================================================
// CONSTANTS
// ============================================================================

#define MAX_ITEM_SIZE_BYTES (1024 * 1024)  // 1MB max per item
#define MIN_CAPACITY 1
#define MAX_CAPACITY 32  // Pathological cases beyond 7±2

/* P2 fix: Minimum exponent for expf() to prevent underflow to 0
 * WHY:  expf(-88.7) ≈ 0 due to float underflow. Clamping to -80 ensures
 *       meaningful decay factor (≈10^-35) while avoiding numerical issues.
 * BIO:  Items untouched for very long should decay to threshold, not 0.
 */
#define MIN_DECAY_EXPONENT (-80.0F)

// ============================================================================
// INTERNAL STRUCTURE
// ============================================================================

/**
 * @brief Internal working memory structure
 *
 * WHAT: Complete working memory state with items, metadata, and statistics
 * WHY:  Encapsulate all data needed for temporal decay and eviction
 * HOW:  Parallel arrays for items, salience, timestamps, and attention flags
 */
struct working_memory {
    // Item storage
    float** items;                  // Array of item pointers
    uint32_t* item_sizes;           // Size of each item in floats

    // Capacity management
    uint32_t capacity;              // Maximum items (default: 7)
    uint32_t current_size;          // Current item count

    // Metadata
    float* salience;                // Importance scores [0.0, 1.0]
    uint64_t* timestamps;           // Last access time (ms)
    bool* attention_refreshed;      // Rehearsal flag (prevents decay)

    // Emotional tagging (Phase 10.3)
    emotional_tag_t* emotions;      // Emotional context for each item
    bool* has_emotion;              // Whether item has emotional tag

    // Configuration
    float decay_tau_ms;             // Decay time constant (default: 1000ms)
    float min_salience;             // Eviction threshold (default: 0.01)
    bool enable_attention_refresh;  // Allow rehearsal to prevent decay
    bool enable_temporal_decay;     // Enable exponential decay

    // Statistics
    uint32_t total_additions;       // Lifetime item additions
    uint32_t total_evictions;       // Lifetime evictions
    uint32_t total_refreshes;       // Lifetime attention refreshes
    uint32_t total_decay_removals;  // Items removed by decay

    // Thread safety (added 2025-11-17)
    nimcp_platform_mutex_t mutex;   // Protects all working memory operations

    // Bio-async integration
    bio_module_context_t bio_ctx;   // Bio-async module context
    bool bio_async_enabled;         // Bio-async registration status

    // Second messenger integration
    second_messenger_system_t* sm_system;  // Second messenger cascade system
    bool enable_second_messengers;         // Whether cascade modulation is enabled
    uint32_t num_neurons;                  // Number of neurons for cascade tracking

    // Global Workspace integration (Phase 10.x)
    global_workspace_t* workspace;           // Global workspace for conscious access
    bool workspace_integration_enabled;       // Workspace integration active
    float workspace_salience_threshold;       // Threshold for triggering ignition

    // Positional encoding integration
    nimcp_pos_encoder_t* pos_encoder;        // Positional encoder for serial position effects
    bool enable_positional_encoding;          // Whether position encoding is active
    nimcp_pos_encoding_type_t pe_type;       // Type of positional encoding
    uint32_t pe_embedding_dim;               // Dimension of position embeddings
    float* pe_buffer;                        // Temporary buffer for position encodings

    // Brain immune integration
    struct brain_immune_system* immune;      // Connected immune system
    bool immune_integration_enabled;         // Immune integration active
    uint32_t inflammation_capacity_penalty;  // Capacity reduction from inflammation
    float last_stress_signal_time_ms;        // Last stress cytokine release time

    // Sleep state integration
    sleep_state_t current_sleep_state;       // Current sleep/wake state for modulation

    // Quantum retrieval integration
    working_memory_quantum_bridge_t* quantum_bridge;  // Quantum search bridge
    bool enable_quantum_wm;                          // Quantum retrieval enabled

    // Internal Knowledge Graph integration (self-awareness)
    kg_module_context_t kg_context;  // KG access context
    bool kg_connected;               // Internal KG is connected

    // SNN and Plasticity bridge integration
    wm_snn_bridge_t* snn_bridge;           // SNN integration bridge
    wm_plasticity_bridge_t* plasticity_bridge;  // Plasticity integration bridge
    bool bridges_enabled;                  // Whether bridges are active
};

// ============================================================================
// ERROR HANDLING
// ============================================================================

static _Thread_local char last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};



// Forward declarations for static functions (SRP split)
static nimcp_error_t handle_wm_store_request( const void* msg, size_t msg_size, nimcp_bio_promise_t response_promise, void* user_data);
static nimcp_error_t handle_wm_retrieve_request( const void* msg, size_t msg_size, nimcp_bio_promise_t response_promise, void* user_data);
static void bio_broadcast_item_stored(working_memory_t* wm, uint32_t slot_id, float salience);
static void bio_broadcast_item_evicted(working_memory_t* wm, uint32_t slot_id);
static void set_error(const char* msg);
static int find_lowest_salience_index(const working_memory_t* wm);
static void evict_item_at_index(working_memory_t* wm, uint32_t index);
static uint64_t get_current_time_ms(void);
static int working_memory_wiring_handler_callback( bio_module_context_t ctx, const bio_message_type_t* message_types, uint32_t message_count, void* user_data );
static void wm_inflammation_callback( brain_immune_system_t* system, const brain_inflammation_site_t* site, void* user_data);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_working_memory_part_helpers.c"  // 9 functions: helpers
#include "nimcp_working_memory_part_accessors.c"  // 20 functions: accessors
#include "nimcp_working_memory_part_processing.c"  // 2 functions: processing
#include "nimcp_working_memory_part_lifecycle.c"  // 3 functions: lifecycle
#include "nimcp_working_memory_part_core.c"  // 18 functions: core
