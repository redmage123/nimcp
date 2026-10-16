#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_nlp.c - Neural Link Protocol Core Implementation
//=============================================================================
/**
 * @file nimcp_nlp.c
 * @brief Core implementation of the Neural Link Protocol
 *
 * WHAT: Main NLP node implementation with lifecycle, messaging, and mode control
 * WHY:  Enable secure, resilient brain-to-brain communication across devices
 * HOW:  Unified interface with mode-specific behavior (Standard/Tactical/Stealth)
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "networking/nlp/nimcp_nlp_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"
#include "utils/rng/nimcp_rand.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "security/nimcp_bbb_helpers.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_timing_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(networking_nlp)

//=============================================================================
// Module Registration
//=============================================================================

#define NLP_MODULE_NAME "neural_link_protocol"

static bool g_nlp_initialized = false;
static nimcp_mutex_t g_nlp_global_mutex;
static nimcp_once_t g_nlp_once = NIMCP_ONCE_INIT;

// Node structure is defined in nimcp_nlp_internal.h

//=============================================================================
// Forward Declarations
//=============================================================================

static void* nlp_recv_thread(void* arg);
static void* networking_nlp_heartbeat_thread(void* arg);
static void* nlp_stealth_thread(void* arg);
static int nlp_process_message(nlp_node_t node, const uint8_t* data, size_t len,
                               const struct sockaddr_in* from);
static int nlp_send_raw(nlp_node_t node, uint32_t peer_id, const uint8_t* data, size_t len);
static void nlp_auto_mode_check(nlp_node_t node);

//=============================================================================
// Bio-Async Integration
//=============================================================================

static bio_module_context_t g_nlp_bio_ctx = NULL;

//=============================================================================
// KG-Driven Wiring Callback (Phase 2: KG-Based Runtime Module Assembly)
//=============================================================================

/**
 * @brief KG-driven wiring handler callback for NLP module
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data NLP node pointer
 * @return 0 on success, -1 on error
 */
static int nlp_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
);

/* Forward declaration for bio handler */
static nimcp_error_t nlp_bio_handler(const void* msg, size_t msg_size,
                                      nimcp_bio_promise_t response_promise,
                                      void* user_data);

/**
 * @brief Broadcast NLP session event to cognitive modules
 */


// Forward declarations for static functions (SRP split)
static inline bool nlp_should_continue(nlp_node_t node);
static inline bool nlp_stealth_active(nlp_node_t node);
static void nlp_register_bio_async(nlp_node_t node);
static void nlp_global_init_once(void);
static bool nlp_global_init(void);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_nlp_part_helpers.c"  // 8 functions: helpers
#include "nimcp_nlp_part_core.c"  // 22 functions: core
#include "nimcp_nlp_part_processing.c"  // 5 functions: processing
#include "nimcp_nlp_part_lifecycle.c"  // 5 functions: lifecycle
#include "nimcp_nlp_part_accessors.c"  // 12 functions: accessors
