/**
 * @file nimcp_bio_router.c
 * @brief Bio-async message router implementation
 *
 * WHAT: Central message routing for inter-module communication
 * WHY:  Decouples modules via message passing with biological semantics
 * HOW:  Per-module inboxes, handler registry, async dispatch, statistics
 *
 * ARCHITECTURE:
 * - Global router singleton with module registry
 * - Per-module context with inbox queue and handler table
 * - Lock-protected module list, lock-free message dispatch where possible
 * - Worker thread pool for async message delivery
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#define LOG_MODULE "bio_router"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_brain_kg.h"  /* Phase 7: KG-driven dispatch */
#include "mesh/nimcp_mesh_bio_integration.h"  /* Phase 15: Mesh integration */
#include "async/nimcp_predictive_protocol.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_cond.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/platform/nimcp_tier_optimization.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

#include <stddef.h>  /* for NULL */
#include "constants/nimcp_timing_constants.h"
// Global BBB system accessor (defined in nimcp_brain_init.c)
extern bbb_system_t nimcp_bbb_get_global_system(void);

/*=============================================================================
 * Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
 *============================================================================*/
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bio_router)

/* Forward declaration: reset subsystem statics on shutdown (defined at end of file) */
static void bio_router_reset_subsystem_statics(void);

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

#define BIO_ROUTER_MAGIC 0x42494F52  // 'BIOR'
#define BIO_MODULE_MAGIC 0x424D4F44  // 'BMOD'
#define MAX_HANDLERS_PER_MODULE 256
#define MAX_INBOX_MESSAGES 16384
#define MAX_WORKER_THREADS 8
#define DEFAULT_TIMEOUT_MS NIMCP_DEFAULT_TIMEOUT_MS
#define MAX_MODULE_NAME 64

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Message queue entry (inbox/outbox)
 */
typedef struct {
    void* msg_data;                 /**< Message buffer (header + payload) */
    size_t msg_size;                /**< Total message size */
    nimcp_bio_promise_t response_promise; /**< Response promise (may be NULL) */
    uint64_t enqueue_time_us;       /**< Timestamp when queued */
} bio_msg_queue_entry_t;

/**
 * @brief Simple ring buffer for messages
 */
typedef struct {
    bio_msg_queue_entry_t* entries;
    uint32_t capacity;
    uint32_t read_idx;
    uint32_t write_idx;
    uint32_t count;
    nimcp_platform_mutex_t mutex;
    nimcp_platform_cond_t not_empty;
    nimcp_platform_cond_t not_full;
} bio_msg_queue_t;

/**
 * @brief Handler registration entry
 */
typedef struct {
    bio_message_type_t msg_type;    /**< Message type handled */
    uint32_t category_mask;         /**< Category mask (0 = specific type) */
    bio_message_handler_t handler;  /**< Handler callback */
    bool is_category_handler;       /**< true if category, false if specific */
} bio_handler_entry_t;

/**
 * @brief Module entry in router registry
 */
typedef struct {
    uint32_t magic;
    bio_module_id_t module_id;
    char module_name[MAX_MODULE_NAME];
    void* user_data;

    bio_msg_queue_t inbox;
    bio_handler_entry_t handlers[MAX_HANDLERS_PER_MODULE];
    uint32_t handler_count;
    nimcp_platform_mutex_t handler_mutex;

    // Statistics
    uint64_t messages_received;
    uint64_t messages_sent;
    uint64_t handler_invocations;
    uint64_t handler_errors;

    // Deadlock prevention: re-entrancy detection flag
    atomic_bool in_handler;           /**< true while handler is executing */
    uint32_t in_handler_source;       /**< source module of message being handled */
} bio_module_entry_t;

/**
 * @brief Module context (opaque handle)
 */
struct bio_module_context_struct {
    uint32_t magic;
    bio_module_entry_t* entry;  /**< Back-reference to registry entry */
};

/**
 * @brief Router global state
 */
struct bio_router_struct {
    uint32_t magic;
    bio_router_config_t config;

    bio_module_entry_t* modules;
    uint32_t module_count;
    uint32_t module_capacity;
    nimcp_platform_mutex_t modules_mutex;

    // Statistics
    bio_router_stats_t stats;
    nimcp_platform_mutex_t stats_mutex;

    // Predictive protocol (optional)
    predictive_protocol_t predictive_proto;

    // Brain immune integration
    void* brain_immune_system;         /**< Brain immune system handle */
    bio_module_context_t immune_ctx;   /**< Immune module context */

    // Dead-letter queue: ring buffer of last 16 dropped message types for diagnostics
    uint32_t dead_letter_types[16];    /**< Ring buffer of dropped message types */
    uint32_t dead_letter_idx;          /**< Write index into dead_letter_types */

    // Message ordering: global sequence counter for causal ordering
    atomic_uint_fast32_t next_sequence_id; /**< Monotonically increasing sequence number */

    unified_mem_manager_t mem_mgr;
    bool initialized;
    bool shutdown_requested;
};

/*=============================================================================
 * GLOBAL STATE
 *============================================================================*/

static struct bio_router_struct* g_router = NULL;
static nimcp_platform_mutex_t g_router_init_mutex;
static nimcp_platform_once_t g_router_init_once = NIMCP_PLATFORM_ONCE_INIT;

/* Orchestrator reference for KG-driven wiring callbacks */
static struct bio_async_orchestrator* g_router_orchestrator = NULL;

/* Brain KG reference for Phase 7: Runtime Message Orchestration */
static struct brain_kg* g_router_brain_kg = NULL;
static nimcp_platform_mutex_t g_router_brain_kg_mutex;
static atomic_bool g_router_brain_kg_mutex_initialized = false;
static bool g_router_brain_kg_mutex_created = false;  /* guards against double pthread_mutex_init */

/* Forward declaration for Phase 7: KG dispatch */
static int bio_router_kg_dispatch_internal(const void* msg, size_t msg_size, uint32_t timeout_ms);

/*=============================================================================
 * PREDICTIVE CODING INTEGRATION
 *============================================================================*/

/**
 * @brief Signal observer entry for predictive coding
 */
typedef struct {
    char signal_name[NIMCP_ID_BUFFER_SIZE];
    float prediction;
    float precision;
    bio_prediction_observer_t callback;
    void* user_data;
    bool active;
} signal_observer_t;

/** Global signal observer registry (simplified) */
static signal_observer_t g_signal_observers[256];
static uint32_t g_signal_observer_count = 0;
static nimcp_platform_mutex_t g_signal_mutex;
static nimcp_platform_once_t g_signal_mutex_once = NIMCP_PLATFORM_ONCE_INIT;

/**
 * WHAT: One-time initialization of signal observer mutex
 * WHY:  Fix TOCTOU race condition in signal observer registration
 * HOW:  Called exactly once via pthread_once before any mutex operations
 */

/*=============================================================================
 * PHASE SYNCHRONIZATION
 *============================================================================*/

/**
 * @brief Phase sync context (simplified implementation)
 */
typedef struct {
    nimcp_oscillation_band_t band;
    nimcp_bio_promise_t* promises;
    uint32_t promise_count;
    uint32_t completed_count;
    nimcp_platform_mutex_t mutex;
    nimcp_platform_cond_t cond;
    bool all_ready;
} phase_sync_context_t;


/*=============================================================================
 * GLIAL WAVE INTEGRATION
 *============================================================================*/

/**
 * @brief Glial wave context
 */
typedef struct {
    uint32_t wave_id;
    bio_module_id_t source_module;
    float intensity;
    float current_intensity;
    uint64_t start_time_us;
    uint8_t metadata[256];
    size_t metadata_size;
    bool active;
} glial_wave_context_t;

/**
 * @brief Wave arrival callback registration
 */
typedef struct {
    bio_module_id_t module_id;
    nimcp_wave_callback_t callback;
    void* user_data;
    bool active;
} wave_callback_entry_t;

/** Global wave state (simplified) */
static glial_wave_context_t g_waves[64];
static uint32_t g_wave_count = 0;
static uint32_t g_next_wave_id = 1;
static wave_callback_entry_t g_wave_callbacks[128];
static uint32_t g_wave_callback_count = 0;
static nimcp_platform_mutex_t g_wave_mutex;
static nimcp_platform_once_t g_wave_mutex_once = NIMCP_PLATFORM_ONCE_INIT;

/**
 * WHAT: One-time initialization of glial wave mutex
 * WHY:  Fix TOCTOU race condition in wave registration
 * HOW:  Called exactly once via pthread_once before any mutex operations
 */

/*=============================================================================
 * SUBSCRIPTION/UNSUBSCRIPTION IMPLEMENTATIONS
 *============================================================================*/

/**
 * @brief Subscription entry for tracking message subscriptions
 */
typedef struct {
    uint32_t msg_type;          /**< Message type subscribed to */
    void* callback;             /**< Handler callback */
    void* user_data;            /**< User context data */
    int channel;                /**< Neuromodulator channel */
    bool active;                /**< Whether subscription is active */
} bio_subscription_entry_t;

#define MAX_SUBSCRIPTIONS 256
static bio_subscription_entry_t g_subscriptions[MAX_SUBSCRIPTIONS];
static uint32_t g_subscription_count = 0;
static nimcp_platform_mutex_t g_subscription_mutex;
static nimcp_platform_once_t g_subscription_once = NIMCP_PLATFORM_ONCE_INIT;

/**
 * @brief Initialize subscription subsystem
 *
 * WHAT: Initialize mutex and state for subscriptions
 * WHY:  Thread-safe subscription management
 * HOW:  Create mutex, clear subscription array (called via pthread_once)
 */

/*=============================================================================
 * BRAIN IMMUNE INTEGRATION
 *============================================================================*/

/**
 * @brief Forward declaration for immune message handler
 *
 * WHAT: Handler for immune-related bio-async messages
 * WHY:  Process cytokine signals and immune coordination messages
 * HOW:  Delegate to brain immune system for processing
 */
static nimcp_error_t bio_immune_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

/**
 * @brief Connect brain immune system to bio-async router
 *
 * WHAT: Register brain immune system with bio-async for cytokine messaging
 * WHY:  Enable immune coordination via bio-async neuromodulator channels
 * HOW:  Register immune module, set up NOREPINEPHRINE channel handlers
 *
 * @param immune_system Brain immune system to connect
 * @return NIMCP_SUCCESS or error
 */

/*=============================================================================
 * BBB EMOTION QUERY REGISTRATION
 *============================================================================*/

/**
 * @brief Emotion query registration entry
 */
typedef struct {
    void* system;               /**< System context pointer */
    char module_name[NIMCP_ID_BUFFER_SIZE];       /**< Module name */
    uint64_t query_count;       /**< Number of queries performed */
    bool active;                /**< Whether registration is active */
} bbb_emotion_registration_t;

#define MAX_EMOTION_REGISTRATIONS 64
static bbb_emotion_registration_t g_emotion_registrations[MAX_EMOTION_REGISTRATIONS];
static uint32_t g_emotion_registration_count = 0;
static nimcp_platform_mutex_t g_emotion_reg_mutex;
static nimcp_platform_once_t g_emotion_reg_once = NIMCP_PLATFORM_ONCE_INIT;

/**
 * @brief Initialize emotion registration subsystem (called via pthread_once)
 */

struct bio_async_orchestrator* bio_router_get_orchestrator(void) {
    return g_router_orchestrator;
}

nimcp_error_t bio_router_register_wiring_callback(
    bio_module_id_t module_id,
    void* callback,
    void* user_data
) {
    /* Silent return when orchestrator not set - this is expected during init
     * when bio-async is not available. Avoids exception spam during brain creation. */
    if (!g_router_orchestrator) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    NIMCP_CHECK_THROW(callback != NULL, NIMCP_ERROR_INVALID_PARAMETER,
                      "bio_router_register_wiring_callback: NULL callback");

    /* Include orchestrator header for register function */
    extern int bio_orchestrator_register_handler_callback(
        struct bio_async_orchestrator* orchestrator,
        bio_module_id_t module_id,
        void* callback,
        void* user_data
    );

    int result = bio_orchestrator_register_handler_callback(
        g_router_orchestrator,
        module_id,
        callback,
        user_data
    );

    if (result == 0) {
        LOG_DEBUG("bio_router_register_wiring_callback: registered callback for module %u",
                  (unsigned)module_id);
        return NIMCP_SUCCESS;
    }

    return NIMCP_ERROR_OPERATION_FAILED;
}

/*=============================================================================
 * BRAIN KNOWLEDGE GRAPH INTEGRATION (Phase 7: Runtime Message Orchestration)
 *============================================================================*/

static struct brain_kg* get_router_brain_kg_safe(void);

struct brain_kg* bio_router_get_brain_kg(void) {
    return get_router_brain_kg_safe();
}

bool bio_router_kg_dispatch_available(void) {
    return (get_router_brain_kg_safe() != NULL);
}

/**
 * @brief Internal: Dispatch message to all KG-discovered handlers
 *
 * WHAT: Route message to all modules that handle this message type
 * WHY:  Enables declarative message routing based on KG wiring
 * HOW:  Query brain_kg for handlers, dispatch to each
 *
 * @param msg Message to dispatch
 * @param msg_size Message size
 * @param timeout_ms Timeout for each dispatch
 * @return Number of modules dispatched to (>= 0), or -1 on error
 *
 * NOTE: This internal function returns int (not nimcp_error_t) to allow
 * distinguishing between "error" (-1) and "zero handlers found" (0).
 * The caller converts -1 to NIMCP_ERROR_OPERATION_FAILED as appropriate.
 */


// Forward declarations for static functions (SRP split)
static void init_router_mutex_once(void);
static nimcp_error_t bio_msg_queue_init(bio_msg_queue_t* queue, uint32_t capacity);
static nimcp_error_t bio_msg_queue_grow(bio_msg_queue_t* queue);
static void bio_msg_queue_destroy(bio_msg_queue_t* queue);
static nimcp_error_t bio_msg_queue_enqueue(bio_msg_queue_t* queue, const void* msg, size_t msg_size, nimcp_bio_promise_t response_promise, uint32_t timeout_ms);
static nimcp_error_t bio_msg_queue_dequeue(bio_msg_queue_t* queue, void** out_msg, size_t* out_size, nimcp_bio_promise_t* out_promise, uint32_t timeout_ms);
static uint32_t bio_msg_queue_count(bio_msg_queue_t* queue);
static bio_module_entry_t* bio_router_find_module(bio_module_id_t module_id);
static nimcp_error_t bio_router_send_with_promise(bio_module_context_t ctx, const void* msg, size_t msg_size, nimcp_bio_promise_t response_promise, uint32_t timeout_ms);
static bio_message_handler_t bio_router_find_handler(bio_module_entry_t* entry, bio_message_type_t msg_type);
static void init_signal_mutex_once(void);
static void init_wave_mutex_once(void);
static void subscription_init(void);
static void emotion_registration_init(void);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_bio_router_part_helpers.c"  // 10 functions: helpers
#include "nimcp_bio_router_part_lifecycle.c"  // 10 functions: lifecycle
#include "nimcp_bio_router_part_stats.c"  // 2 functions: stats
#include "nimcp_bio_router_part_accessors.c"  // 8 functions: accessors
#include "nimcp_bio_router_part_core.c"  // 20 functions: core
#include "nimcp_bio_router_part_processing.c"  // 6 functions: processing
