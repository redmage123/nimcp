/**
 * @file nimcp_distributed_cognition.c
 * @brief Implementation of distributed cognitive integration layer
 *
 * ARCHITECTURE:
 * - Mediator between local cognitive systems and P2P network
 * - Event-driven coordination with thread-safe synchronization
 * - Supports neuromodulator diffusion, glial coordination, region sync
 * - ASYNC INTEGRATION: Uses futures for non-blocking operations
 * - CONFIG INTEGRATION: All hyperparameters configurable via config module
 * - SECURITY INTEGRATION: Registered with security system for monitoring
 *
 * THREAD MODEL:
 * - 3 worker threads (neuromod sync, glial sync, region sync)
 * - rwlock protects shared state (readers-writer lock)
 * - Lock-free reads for high-frequency queries
 *
 * MEMORY MANAGEMENT:
 * - All allocations use unified memory (nimcp_malloc/calloc/free)
 * - Cleanup on destroy, NULL-safe operations
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version Phase 3.1 (Async/Config/Logging/Security Integrated)
 */

#include "networking/distributed/nimcp_distributed_cognition.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_future.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/error/nimcp_error_codes.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_security.h"
#include "utils/config/nimcp_dynamic_config.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/validation/nimcp_common.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "networking.distributed"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(distributed_cognition)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Registered neuromodulator pool for synchronization
 */
typedef struct {
    neuromodulator_pool_t* pool;   /**< Local neuromod pool */
    uint64_t last_sync_time;        /**< Last sync timestamp */
    bool needs_broadcast;           /**< Flag for pending broadcast */
} registered_neuromod_t;

/**
 * @brief Registered glial integration system
 */
typedef struct {
    glial_integration_t* glial;    /**< Local glial system */
    uint64_t last_sync_time;        /**< Last sync timestamp */
    uint32_t pending_prunings;      /**< Pending pruning operations */
} registered_glial_t;

/**
 * @brief Registered brain region
 */
typedef struct {
    brain_region_t* region;        /**< Local brain region */
    uint16_t region_type;           /**< Region type identifier */
    uint64_t last_sync_time;        /**< Last sync timestamp */
} registered_region_t;

/**
 * @brief Distributed cognition coordinator (opaque struct)
 */
struct distrib_cognition_struct {
    // Configuration
    distrib_cognition_config_t config;

    // P2P network
    p2p_node_t p2p_node;

    // Registered systems
    registered_neuromod_t* neuromod_pools;
    size_t neuromod_pool_count;
    size_t neuromod_pool_capacity;

    registered_glial_t* glial_systems;
    size_t glial_system_count;
    size_t glial_system_capacity;

    registered_region_t* brain_regions;
    size_t brain_region_count;
    size_t brain_region_capacity;

    // Statistics
    distrib_cognition_stats_t stats;

    // Thread safety
    nimcp_rwlock_t rwlock;

    // Worker threads
    nimcp_thread_t neuromod_thread;
    nimcp_thread_t glial_thread;
    nimcp_thread_t region_thread;

    // Control flags
    bool running;
    bool shutdown_requested;

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

//=============================================================================
// Module Registration and Configuration
//=============================================================================

#define MODULE_NAME "networking.distributed_cognition"
static uint32_t g_security_module_id = 0;

// Configuration keys
#define CONFIG_KEY_ENABLE_NEUROMOD_SYNC "distrib_cog.enable_neuromod_sync"
#define CONFIG_KEY_NEUROMOD_INTERVAL "distrib_cog.neuromod_broadcast_interval_ms"
#define CONFIG_KEY_NEUROMOD_DIFFUSION "distrib_cog.neuromod_diffusion_rate"
#define CONFIG_KEY_ENABLE_GLIAL_SYNC "distrib_cog.enable_glial_sync"
#define CONFIG_KEY_GLIAL_INTERVAL "distrib_cog.glial_sync_interval_ms"
#define CONFIG_KEY_ENABLE_REGION_SYNC "distrib_cog.enable_region_sync"
#define CONFIG_KEY_REGION_INTERVAL "distrib_cog.region_sync_interval_ms"
#define CONFIG_KEY_MAX_MESSAGE_QUEUE "distrib_cog.max_message_queue"

//=============================================================================
// Default Configuration
//=============================================================================

static const distrib_cognition_config_t DEFAULT_CONFIG = {
    .enable_neuromod_sync = true,
    .neuromod_broadcast_interval_ms = 100,  // 10 Hz broadcast
    .neuromod_diffusion_rate = 0.1F,        // 10% cross-node diffusion

    .enable_glial_sync = true,
    .glial_sync_interval_ms = 500,          // 2 Hz sync

    .enable_region_sync = true,
    .region_sync_interval_ms = 200,         // 5 Hz sync

    .sync_mode = SYNC_MODE_BIDIRECTIONAL,
    .max_message_queue = 1000,
    .enable_bio_async = false
};

//=============================================================================
// Forward Declarations - Worker Threads
//=============================================================================

static void* neuromod_sync_worker(void* arg);
static void* glial_sync_worker(void* arg);
static void* region_sync_worker(void* arg);

//=============================================================================
// Forward Declarations - Message Handlers
//=============================================================================

static void handle_neuromod_level_msg(distrib_cognition_t dc, const uint8_t* payload, size_t payload_len);
static void handle_glial_pruning_msg(distrib_cognition_t dc, const uint8_t* payload, size_t payload_len);
static void handle_glial_calcium_msg(distrib_cognition_t dc, const uint8_t* payload, size_t payload_len);
static void handle_region_activity_msg(distrib_cognition_t dc, const uint8_t* payload, size_t payload_len);

//=============================================================================
// Creation and Destruction
//=============================================================================


//=============================================================================
// Async Operations - Context Structures
//=============================================================================

/**
 * @brief Context for async neuromodulator broadcast
 */
typedef struct {
    distrib_cognition_t dc;
    neuromodulator_type_t type;
    float concentration;
    nimcp_promise_t promise;
} async_neuromod_broadcast_ctx_t;

/**
 * @brief Context for async calcium wave propagation
 */
typedef struct {
    distrib_cognition_t dc;
    uint32_t astrocyte_id;
    float calcium_level;
    float wave_velocity;
    nimcp_promise_t promise;
} async_calcium_wave_ctx_t;

/**
 * @brief Context for async pruning coordination
 */
typedef struct {
    distrib_cognition_t dc;
    uint32_t source_neuron_id;
    uint32_t target_neuron_id;
    float activity_score;
    uint8_t action;
    nimcp_promise_t promise;
} async_pruning_ctx_t;

//=============================================================================
// Async Operations - Worker Threads
//=============================================================================

/**
 * @brief Worker thread for async neuromodulator broadcast
 */


// Forward declarations for static functions (SRP split)
static void load_configuration(distrib_cognition_config_t* config);
static void* async_neuromod_broadcast_worker(void* arg);
static void* async_calcium_wave_worker(void* arg);
static void* async_pruning_worker(void* arg);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_distributed_cognition_part_accessors.c"  // 3 functions: accessors
#include "nimcp_distributed_cognition_part_lifecycle.c"  // 2 functions: lifecycle
#include "nimcp_distributed_cognition_part_core.c"  // 12 functions: core
#include "nimcp_distributed_cognition_part_helpers.c"  // 10 functions: helpers
