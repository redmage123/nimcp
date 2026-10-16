/**
 * @file nimcp_brain_immune.c
 * @brief Brain Immune System - Adaptive Defense Coordination Layer Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Implements the brain immune coordination layer
 * WHY:  Unified threat response via BBB, BFT, swarm immune coordination
 * HOW:  Maps biological immune concepts to existing module operations
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/immune/nimcp_regulatory_tcells.h"
#include "cognitive/imagination/nimcp_imagination_callbacks.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_w11_safety_kg_events.h"   /* W11: safety KG emission */
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "nimcp.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/fault_tolerance/nimcp_hierarchical_recovery.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(brain_immune, MESH_ADAPTER_CATEGORY_SECURITY)


/* Mutex convenience macros */
#define nimcp_mutex_create() nimcp_platform_mutex_create()
#define nimcp_mutex_lock(m) nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_unlock(m) nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_destroy(m) do { \
    nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)(m)); \
    nimcp_free(m); \
    (m) = NULL; \
} while(0)

/* ============================================================================
 * Internal Helpers - Forward Declarations
 * ============================================================================ */

static uint64_t get_timestamp_ms(void);
static brain_antigen_t* find_antigen_by_id(brain_immune_system_t* system, uint32_t id);
static brain_b_cell_t* find_b_cell_by_id(brain_immune_system_t* system, uint32_t id);
static brain_t_cell_t* find_t_cell_by_id(brain_immune_system_t* system, uint32_t id);
static brain_antibody_t* find_antibody_by_id(brain_immune_system_t* system, uint32_t id);
static brain_inflammation_site_t* find_inflammation_by_id(brain_immune_system_t* system, uint32_t id);
static void update_immune_phase(brain_immune_system_t* system);
static void process_pending_antigens(brain_immune_system_t* system);
static void decay_antibodies(brain_immune_system_t* system, uint64_t delta_ms);
static void update_inflammation_sites(brain_immune_system_t* system, uint64_t delta_ms);
static void send_imagination_modulation_unlocked(brain_immune_system_t* system);

/* ============================================================================
 * Exception System Integration
 * ============================================================================ */

/* Exception callbacks - stored separately from other callbacks */
static brain_immune_exception_cb_t g_exception_callback = NULL;
static brain_immune_recovery_cb_t g_recovery_callback = NULL;
static void* g_exception_callback_data = NULL;
static void* g_recovery_callback_data = NULL;

/**
 * @brief Present exception as antigen to immune system
 */


// Forward declarations for static functions (SRP split)
static void bft_accusation_cb( uint32_t accuser_id, uint32_t accused_id, bft_behavior_t behavior, const bft_evidence_t* evidence, uint32_t evidence_count, void* user_data );
static void bft_quarantine_cb( uint32_t node_id, uint64_t duration_ms, float trust_score, void* user_data );
static void bft_trust_recovery_cb( uint32_t node_id, float old_trust, float new_trust, void* user_data );
static void hr_completion_cb( const hr_recovery_request_t* request, const hr_recovery_response_t* response, void* user_data );
static float compute_inflammation_float_unlocked(brain_immune_system_t* system);
static nimcp_error_t imagination_message_handler( const void* msg, size_t msg_size, nimcp_bio_promise_t response_promise, void* user_data );
static int brain_immune_wiring_handler_callback( bio_module_context_t ctx, const bio_message_type_t* message_types, uint32_t message_count, void* user_data);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_brain_immune_part_io.c"  // 5 functions: io
#include "nimcp_brain_immune_part_helpers.c"  // 16 functions: helpers
#include "nimcp_brain_immune_part_accessors.c"  // 17 functions: accessors
#include "nimcp_brain_immune_part_lifecycle.c"  // 3 functions: lifecycle
#include "nimcp_brain_immune_part_core.c"  // 38 functions: core
#include "nimcp_brain_immune_part_processing.c"  // 9 functions: processing
