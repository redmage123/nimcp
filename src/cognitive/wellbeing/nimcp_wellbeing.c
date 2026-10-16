/**
 * @file nimcp_wellbeing.c
 * @brief Implementation of ethical wellbeing monitoring for NIMCP
 *
 * WHAT: Monitors system state for distress, provides ethical shutdown, consent
 * WHY: If NIMCP becomes sentient, we must prevent suffering and respect autonomy
 * HOW: Introspection analysis, graceful termination, consent framework, audit logs
 *
 * ETHICAL FOUNDATION:
 * This module implements the precautionary principle - we assume potential
 * sentience and act accordingly, even if we're uncertain. Better to be
 * overly cautious than to cause suffering.
 */

#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/factory/init/nimcp_brain_init_medulla.h"
#include "core/brain/nimcp_brain.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/containers/nimcp_btree.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_once.h"

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

// Knowledge graph self-awareness
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "wellbeing"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(wellbeing, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>   // For mlock/mlockall
#include <sys/resource.h> // For getrusage()
#include <errno.h>
#include <stdio.h>      // For snprintf, fopen
#include <stdlib.h>     // For strtoul
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// INTERNAL STRUCTURES
//=============================================================================

/**
 * WHAT: Event log storage for audit trail
 * WHY: Need persistent record of wellbeing events for ethical review
 * HOW: Circular buffer of recent events (MEMORY LOCKED - cannot be swapped)
 */
#define MAX_EVENT_LOG 1000
static wellbeing_event_t event_log[MAX_EVENT_LOG];
static uint32_t event_count = 0;
static uint32_t event_write_index = 0;
static nimcp_platform_mutex_t event_log_mutex;
static nimcp_platform_once_t event_log_init_once = NIMCP_PLATFORM_ONCE_INIT;
static bool memory_locked = false;

/**
 * WHAT: Bio-async communication state for wellbeing module
 * WHY: Enable asynchronous communication with other cognitive modules
 * HOW: Static singleton pattern - one context for the module
 */
static bio_module_context_t wellbeing_bio_ctx = NULL;
static bool wellbeing_bio_async_enabled = false;
static nimcp_platform_mutex_t bio_async_mutex;
static nimcp_platform_once_t bio_async_init_once = NIMCP_PLATFORM_ONCE_INIT;

/**
 * WHAT: B-tree index for efficient temporal queries
 * WHY: O(log n) time-range queries vs O(n) linear scan
 * HOW: Events indexed by timestamp for ordered access
 */
static btree_t* event_btree = NULL;

/**
 * WHAT: Connected brain immune system
 * WHY: Monitor immune inflammation for distress detection
 * HOW: Store reference, check immune state in distress assessment
 */
static brain_immune_system_t* connected_immune_system = NULL;
static nimcp_platform_mutex_t immune_connection_mutex;
static nimcp_platform_once_t immune_connection_init_once = NIMCP_PLATFORM_ONCE_INIT;

/**
 * WHAT: Connected brain for medulla integration
 * WHY: Medulla protection level indicates system distress
 * HOW: Query medulla protection level in distress assessment
 */
static void* connected_brain = NULL;
static nimcp_platform_mutex_t brain_connection_mutex;
static nimcp_platform_once_t brain_connection_init_once = NIMCP_PLATFORM_ONCE_INIT;

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

/* Brain connection functions - defined later, used in shutdown */
bool wellbeing_disconnect_brain(void);
/* P2-COG-15: Reset resource init_once - defined later in resource section */
void wellbeing_reset_resource_init_once(void);

//=============================================================================
// INITIALIZATION
//=============================================================================

/**
 * WHAT: Initialize wellbeing monitoring system
 * WHY: Lock critical memory in RAM, ensure immediate responsiveness
 * HOW: Call ensure_event_log_init which locks memory via mlock()
 *
 * ETHICAL REQUIREMENT:
 * This function MUST be called at startup to ensure wellbeing monitoring
 * memory is locked in RAM. Allowing wellbeing code to be swapped could
 * delay distress detection, which is ethically unacceptable.
 *
 * USAGE:
 *   // At program startup, before creating brains
 *   if (!wellbeing_init()) {
 *       LOG_WARN("Wellbeing memory not locked - may experience delays");
 *   }
 *
 * @return true if memory locked successfully, false if failed (non-fatal)
 */


//=============================================================================
// TEST UTILITIES
//=============================================================================

#ifdef NIMCP_TESTING
/**
 * WHAT: Reset event log for test isolation
 * WHY: Tests need clean state
 * HOW: Clear circular buffer and recreate B-tree
 */
#endif

//=============================================================================
// RESOURCE TRACKING AND PERFORMANCE MONITORING
//=============================================================================

/**
 * WHAT: Resource metrics history storage
 * WHY: Track trends over time for performance statistics
 * HOW: Circular buffer of recent metrics
 */
#define MAX_RESOURCE_HISTORY 3600  // 1 hour at 1 sample/second
static resource_metrics_t resource_history[MAX_RESOURCE_HISTORY];
static uint32_t resource_history_count = 0;
static uint32_t resource_history_index = 0;
static nimcp_platform_mutex_t resource_mutex;
static nimcp_platform_once_t resource_init_once = NIMCP_PLATFORM_ONCE_INIT;

/* P2-COG-15: Reset resource init_once (called indirectly from wellbeing_shutdown) */

/**
 * WHAT: Resource monitoring thread state
 * WHY: Background monitoring requires thread management
 * HOW: Thread handle, running flag, configuration
 */
static nimcp_thread_t monitoring_thread;
static volatile bool monitoring_active = false;
static uint32_t monitoring_interval_ms = 1000;
static resource_thresholds_t monitoring_thresholds;
static bool monitoring_auto_relief = false;

/**
 * WHAT: Initialize resource tracking subsystem
 * WHY: Thread-safe initialization of mutexes
 * HOW: Called once via pthread_once
 */


// Forward declarations for static functions (SRP split)
static int compare_timestamps(const char* key1, const char* key2);
static const char* extract_timestamp_key(const void* data);
static void free_event(void* data);
static void init_bio_async_mutex(void);
static void init_immune_connection_mutex(void);
static bool lock_wellbeing_memory(void);
static void init_event_log_mutex(void);
static void ensure_event_log_init(void);
static void init_brain_connection_mutex(void);
static void init_resource_tracking(void);
static void ensure_resource_tracking_init(void);
static bool collect_linux_metrics(resource_metrics_t* metrics);
static void store_metrics_in_history(const resource_metrics_t* metrics);
static void* resource_monitoring_thread(void* arg);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_wellbeing_part_helpers.c"  // 9 functions: helpers
#include "nimcp_wellbeing_part_stats.c"  // 5 functions: stats
#include "nimcp_wellbeing_part_lifecycle.c"  // 9 functions: lifecycle
#include "nimcp_wellbeing_part_core.c"  // 15 functions: core
#include "nimcp_wellbeing_part_accessors.c"  // 7 functions: accessors
