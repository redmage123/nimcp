/**
 * @file nimcp_systems_consolidation.c
 * @brief Phase M2: Systems consolidation implementation
 *
 * WHAT: Implements hippocampus → cortex memory transfer during sleep
 * WHY:  Models complementary learning systems (McClelland et al., 1995)
 * HOW:  Sleep replay drives cortical plasticity and semantic abstraction
 *
 * BIO-ASYNC INTEGRATION:
 * - Module ID: 0x0332 (BIO_MODULE_SYSTEMS_CONSOLIDATION)
 * - Publishes: replay events, consolidation progress
 * - Subscribes: sleep state changes, engram updates
 *
 * @version Phase M2
 * @date 2025-11-13
 */

#define LOG_MODULE "systems_consolidation"

#include "cognitive/memory/nimcp_systems_consolidation.h"
#include "cognitive/memory/nimcp_memory_kg_events.h"  /* W6: KG event emitters */
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "cognitive/memory/nimcp_engram.h"
#include <stddef.h>  /* for NULL */
#include "nimcp.h"  // For NIMCP_ERROR_* codes
#include "utils/exception/nimcp_exception_macros.h"  // Phase 7: Exception integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/logging/nimcp_logging.h"
#include "utils/containers/nimcp_vector.h"
#include <string.h>
#include <math.h>
#include <stdatomic.h>

// Use unique prefix to avoid conflict with cognitive/consolidation/nimcp_consolidation.c
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE(systems_consolidation, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// BIO-ASYNC MODULE REGISTRATION
//=============================================================================

#define BIO_MODULE_SYSTEMS_CONSOLIDATION 0x0332

//=============================================================================
// BIO-ASYNC HANDLERS (Forward declarations)
//=============================================================================

static nimcp_error_t handle_consolidation_trigger(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static void bio_broadcast_consolidation_complete(systems_consolidation_system_t* system, uint32_t engram_id, float strength);


// Forward declarations for static functions (SRP split)
static int systems_consolidation_wiring_handler_callback( bio_module_context_t ctx, const bio_message_type_t* message_types, uint32_t message_count, void* user_data );
static uint64_t generate_node_id(void);
static cortical_memory_node_t* cortical_node_create( const float* features, uint32_t feature_dim, uint64_t source_engram_id);
static void cortical_node_destroy(cortical_memory_node_t* node);
static bool cortical_node_add_neighbor( cortical_memory_node_t* node, cortical_memory_node_t* neighbor, float strength);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_systems_consolidation_part_processing.c"  // 4 functions: processing
#include "nimcp_systems_consolidation_part_helpers.c"  // 4 functions: helpers
#include "nimcp_systems_consolidation_part_lifecycle.c"  // 5 functions: lifecycle
#include "nimcp_systems_consolidation_part_core.c"  // 6 functions: core
#include "nimcp_systems_consolidation_part_accessors.c"  // 5 functions: accessors
