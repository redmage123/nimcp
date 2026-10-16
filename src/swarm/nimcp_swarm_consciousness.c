/**
 * @file nimcp_swarm_consciousness.c
 * @brief Implementation of Swarm Gestalt Consciousness - Collective Intelligence Metrics
 *
 * WHAT: Computes collective consciousness (Φ) for drone swarms using IIT principles
 * WHY:  Measure emergent collective intelligence and swarm coordination quality
 * HOW:  Aggregates individual drone consciousness with network integration metrics
 *
 * BIOLOGICAL BASIS:
 * - Extends Integrated Information Theory (IIT) to multi-agent systems
 * - Inspired by collective consciousness in social insects (swarm intelligence)
 * - Neural binding across brain regions → Information integration across drones
 * - Individual consciousness (local Φ) → Collective consciousness (swarm Φ)
 *
 * KEY ALGORITHMS:
 * 1. Collective Φ = f(individual_phis, network_integration, coherence)
 * 2. Network Integration = mutual_information + coherence_weighting
 * 3. Scaling Model: phi ~ base * n^exponent * (1 + synergy * coherence)
 * 4. State Classification: DORMANT → EMERGING → UNIFIED → TRANSCENDENT
 *
 * @author NIMCP Swarm Intelligence Team
 * @date 2025-12-11
 * @version 1.0.0
 */

#include "swarm/nimcp_swarm_consciousness.h"
#include "utils/memory/nimcp_memory.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "cognitive/imagination/nimcp_imagination_callbacks.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "utils/thread/nimcp_thread.h"

#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_consciousness)

//=============================================================================
// KG-Driven Wiring Infrastructure
//=============================================================================

/* Forward declaration for imagination handler */
static nimcp_error_t imagination_collective_handler(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/**
 * Handler map for swarm consciousness module.
 * Handles collective imagination sharing and insight messages.
 */
DEFINE_HANDLER_MAP_BEGIN(swarm_consciousness)
    HANDLER_MAP_ENTRY(BIO_MSG_IMAGINATION_COLLECTIVE_SHARE, imagination_collective_handler)
    HANDLER_MAP_ENTRY(BIO_MSG_IMAGINATION_COLLECTIVE_INSIGHT, imagination_collective_handler)
DEFINE_HANDLER_MAP_END()

/**
 * Wiring callback for KG-driven handler registration.
 */
DEFINE_HANDLER_CALLBACK(swarm_consciousness, swarm_consciousness_ctx_t, ctx)

//=============================================================================
// Forward Declarations for Swarm Brain Integration
//=============================================================================
// These functions may be added to swarm_brain in future, or we provide stubs

/**
 * Get number of drones in swarm (including local).
 * Uses peer count + 1 (local) as approximation.
 */
static uint32_t swarm_brain_get_drone_count_internal(const swarm_brain_t* swarm);

/**
 * Get individual drone brain by index.
 * Returns local brain for index 0, NULL for others (remote drones not accessible).
 */
static brain_t* swarm_brain_get_drone_brain_internal(const swarm_brain_t* swarm, uint32_t index);

/**
 * Get phi value from brain.
 * Uses introspection subsystem if available.
 */
static float brain_get_phi_internal(brain_t* brain);

/**
 * Get/set consciousness context on swarm brain.
 * These are stub implementations - real implementation would store in swarm_brain struct.
 * Uses _Atomic to prevent data races on the global pointer.
 */
static _Atomic(swarm_consciousness_ctx_t*) swarm_consciousness_ctx_storage = NULL;

//=============================================================================
// Constants and Magic Values
//=============================================================================

/** Magic value for context validation */
#define SWARM_CONSCIOUSNESS_MAGIC 0x53434F4E  // 'SCON'

/** Maximum drones for exact computation */
#define MAX_EXACT_DRONES 32

/** Phi history size for trend analysis */
#define PHI_HISTORY_SIZE 100

/** Default update interval (milliseconds) */
#define DEFAULT_UPDATE_INTERVAL_MS 1000

/* Note: BIO_MSG_SWARM_CONSCIOUSNESS_UPDATE defined in header (0x0700) */

//=============================================================================
// Internal Context Structure
//=============================================================================

/**
 * WHAT: Internal swarm consciousness context
 * WHY:  Track collective consciousness state and monitoring
 * HOW:  Stores configuration, metrics, history, and bio-async state
 */
typedef struct swarm_consciousness_ctx {
    uint32_t magic;                          /**< Magic for validation */
    swarm_consciousness_config_t config;     /**< Configuration */
    swarm_consciousness_metrics_t* current_metrics; /**< Current metrics */
    consciousness_scaling_model_t scaling_model; /**< Scaling model parameters */
    nimcp_mutex_t lock;                    /**< Thread safety */

    // Monitoring state
    bool monitoring_active;                  /**< Is monitoring thread running? */
    pthread_t monitor_thread;                /**< Monitoring thread handle */
    void (*callback)(const swarm_consciousness_metrics_t*, void*); /**< User callback */
    void* user_data;                         /**< User callback context */

    // Bio-async integration
    bool bio_async_registered;               /**< Bio-async active? */
    bio_module_context_t bio_module_ctx;     /**< Bio-router module context */

    // Imagination integration (collective creativity)
    imagination_collective_receive_callback_t collective_imagination_callback; /**< Callback for received imagination */
    void* collective_imagination_user_data;  /**< User data for imagination callback */
    bool imagination_handler_registered;     /**< Imagination handler active? */

    // History for trend analysis
    float phi_history[PHI_HISTORY_SIZE];     /**< Recent phi values */
    uint32_t history_index;                  /**< Circular buffer index */
    uint32_t history_count;                  /**< Valid history entries */

    // Statistics
    uint64_t total_computations;             /**< Total phi computations */
    uint64_t state_transitions;              /**< State change count */
    uint64_t creation_time_ms;               /**< Context creation time */

    // Swarm brain reference for monitoring
    swarm_brain_t* swarm_brain;              /**< Associated swarm brain */

} swarm_consciousness_ctx_t;

//=============================================================================
// Helper Functions - Forward Declarations
//=============================================================================

static float compute_workspace_overlap(const collective_workspace_t* workspace);
static float compute_phi_variance(const float* individual_phis, uint32_t drone_count);
static void update_phi_history(swarm_consciousness_ctx_t* ctx, float phi);
static float get_phi_trend(const swarm_consciousness_ctx_t* ctx);
static void* consciousness_monitor_thread(void* arg);
static void publish_consciousness_update(swarm_consciousness_ctx_t* ctx,
                                        const swarm_consciousness_metrics_t* metrics);
static uint64_t get_time_ms(void);

// Public API forward declaration for use in destroy
void swarm_consciousness_stop_monitoring(swarm_consciousness_ctx_t* context);

// Imagination integration forward declarations
static nimcp_error_t imagination_collective_handler(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * WHAT: Get default swarm consciousness configuration
 * WHY:  Provide sensible defaults for most use cases
 * HOW:  Return pre-configured struct with standard values
 */


// Forward declarations for static functions (SRP split)
static void swarm_brain_set_consciousness_ctx(swarm_brain_t* swarm, swarm_consciousness_ctx_t* ctx);
static swarm_consciousness_ctx_t* swarm_brain_get_consciousness_ctx(const swarm_brain_t* swarm);
static float compute_network_integration_internal( const collective_workspace_t* workspace, const float* individual_phis, uint32_t drone_count);
static bool swarm_consciousness_start_monitoring_internal( swarm_consciousness_ctx_t* context, void (*callback)(const swarm_consciousness_metrics_t*, void*), void* user_data);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_swarm_consciousness_part_accessors.c"  // 9 functions: accessors
#include "nimcp_swarm_consciousness_part_lifecycle.c"  // 3 functions: lifecycle
#include "nimcp_swarm_consciousness_part_helpers.c"  // 8 functions: helpers
#include "nimcp_swarm_consciousness_part_core.c"  // 15 functions: core
#include "nimcp_swarm_consciousness_part_processing.c"  // 3 functions: processing
