/**
 * @file nimcp_introspection.c
 * @brief Implementation of brain introspection and state access
 *
 * WHAT: Implements APIs to query internal neural network state, neuron
 * activity, uncertainty estimation, pattern tracking, and topology analysis.
 *
 * WHY: Provides the window into brain internals needed for metacognition,
 * explanation, debugging, and conscious self-awareness.
 *
 * HOW: Accesses internal neural network structures, computes statistics,
 * tracks patterns, estimates uncertainty via ensemble methods, and maintains
 * activity history. Thread-safe with mutex protection.
 *
 * DESIGN PATTERNS:
 * - Factory: Context creation/destruction
 * - Strategy: Different state extraction strategies
 * - Observer: State change callbacks
 * - Memento: Activity history snapshots
 * - Facade: Simplified access to complex internals
 *
 * THREAD SAFETY: All public functions are thread-safe via mutex protection.
 * Internal state is protected by context->lock.
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/introspection/nimcp_ensemble_uncertainty.h"
#include "cognitive/introspection/nimcp_introspection_snn_bridge.h"
#include "cognitive/introspection/nimcp_introspection_plasticity_bridge.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "core/brain/factory/init/nimcp_brain_init_medulla.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/algorithms/nimcp_monte_carlo.h"  /* P1-COG-02: mc_random_uniform, mc_seed_from_time */
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/containers/nimcp_queue.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/containers/nimcp_vector.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/statistics/nimcp_statistics.h"

// Bio-async messaging infrastructure
#include "nimcp.h"  // For error codes
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"

// Internal Knowledge Graph integration
#include "core/brain/nimcp_brain_kg_helpers.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/executive/nimcp_w9kg_events.h"  // W9-kg: KG event + read helpers

#define LOG_MODULE "introspection"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(introspection, MESH_ADAPTER_CATEGORY_COGNITIVE)


// Phase 10.3: Emotional working memory integration
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"

/* ========================================================================
 * INTERNAL STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Pattern registry entry
 * WHY: Track learned patterns and their activity
 * HOW: Hash table of pattern metadata
 */
typedef struct pattern_entry {
    char* name;                 /* Pattern identifier */
    float current_activity;     /* Current activation */
    float activity_sum;         /* Sum for average */
    uint32_t activation_count;  /* Count for average */
    float pattern_strength;     /* Learning strength */
    uint64_t first_learned;     /* First occurrence */
    uint64_t last_activated;    /* Last occurrence */
    struct pattern_entry* next; /* Hash table chain */
} pattern_entry_t;

/**
 * WHAT: Pattern registry (hash table)
 * WHY: Fast O(1) lookup of patterns by name
 * HOW: Chained hash table with 256 buckets
 */
typedef struct {
    pattern_entry_t* buckets[256]; /* Hash buckets */
    uint32_t num_patterns;         /* Total patterns */
    nimcp_mutex_t lock;            /* Thread safety */
} pattern_registry_t;

/**
 * WHAT: Introspection context structure (Pimpl)
 * WHY: Encapsulate implementation details
 * HOW: Opaque pointer pattern
 *
 * REFACTORING NOTE (activity history):
 * - Replaced custom circular buffer (activity_history_buffer_t) with nimcp_queue
 * - WHY: Eliminates ~50 lines of code duplication, standardized API
 * - BENEFITS: Blocking operations, better statistics, peek/clear, thread-safe
 * - NO PERFORMANCE IMPACT: Both use mutex (not lock-free)
 */
struct introspection_context_struct {
    brain_t brain;                 /* Associated brain */
    introspection_config_t config; /* Configuration */

    /* Bio-async module context */
    bio_module_context_t bio_ctx;  /* Message router context */
    bool bio_async_enabled;        /* Bio-async registration status */

    /* Pattern tracking */
    pattern_registry_t* pattern_registry; /* Learned patterns */

    /* Activity history - now using standardized queue utility */
    nimcp_queue_handle_t activity_queue; /* Activity snapshots queue */

    /* Auto activity history state */
    uint64_t last_sample_time_ms;        /* Last sampling timestamp */
    activity_sample_callback_t sample_callback; /* Registered callback */
    void* sample_callback_context;       /* User context for callback */
    float last_avg_activation;           /* Previous avg for change detection */

    /* Network topology cache */
    network_topology_t topology; /* Cached topology */
    bool topology_cached;        /* Is topology valid? */

    /* Statistics */
    introspection_stats_t stats; /* Performance stats */

    /* Ensemble uncertainty (Phase: Real Ensemble Implementation) */
    ensemble_context_t ensemble; /* Ensemble for true uncertainty estimation */

    /* Brain immune system integration */
    struct brain_immune_system* immune_system; /* Connected immune system (optional) */

    /* Internal Knowledge Graph integration */
    kg_module_context_t kg_context; /* KG access context */
    bool kg_connected;              /* Internal KG is connected */

    /* Thread safety */
    nimcp_mutex_t lock; /* Protects context */

    /* SNN and Plasticity bridges */
    introspection_snn_bridge_t* snn_bridge;
    introspection_plasticity_bridge_t* plasticity_bridge;
    bool bridges_enabled;

    /* P1-COG-02: Thread-safe RNG seed for uncertainty estimation */
    uint32_t rand_seed;
};

/* ========================================================================
 * BIO-ASYNC MESSAGE HANDLERS
 * ======================================================================== */

/**
 * @brief KG-driven wiring callback for introspection module
 */
static int introspection_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data);

/* Forward declaration of handler */
static nimcp_error_t handle_introspection_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data);

/* ========================================================================
 * FORWARD DECLARATIONS
 * ======================================================================== */

static uint32_t hash_string(const char* str);
static pattern_entry_t* pattern_registry_lookup(pattern_registry_t* registry, const char* name);
static void pattern_registry_update(pattern_registry_t* registry, const char* name, float activity);
static float compute_introspection_entropy(const float* values, uint32_t count);
static float compute_cosine_similarity(const float* a, const float* b, uint32_t dimension);

/* ========================================================================
 * CONFIGURATION
 * ======================================================================== */

/**
 * WHAT: Get default introspection configuration
 * WHY: Sensible defaults for most use cases
 * HOW: Return pre-configured struct
 */

/**
 * WHAT: Get immune system from introspection context
 * WHY: Access immune system for external queries
 * HOW: Return stored reference
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
struct brain_immune_system* introspection_get_immune(introspection_context_t context)
{
    /* WHAT: Validate input */
    if (context == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_get_immune: validation failed");
        return NULL;
    }

    /* WHAT: Get immune system with thread safety */
    nimcp_mutex_lock(&context->lock);
    struct brain_immune_system* immune = context->immune_system;
    nimcp_mutex_unlock(&context->lock);

    return immune;
}

/* ========================================================================
 * HELPER FUNCTIONS
 * ======================================================================== */


/**
 * WHAT: Compute Shannon entropy of values
 * WHY: Measure information content
 * HOW: Normalize values to probabilities, delegate to central stats
 *
 * Uses nimcp_stats_entropy() from utils/statistics for core computation.
 *
 * COMPLEXITY: O(n)
 */
static float compute_introspection_entropy(const float* values, uint32_t count)
{
    if (values == NULL || count == 0) {
        return 0.0F;
    }

    /* WHAT: Compute sum of absolute values for normalization */
    float sum = 0.0F;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            introspection_heartbeat("introspectio_loop",
                             (float)(i + 1) / (float)count);
        }
        sum += fabsf(values[i]);
    }

    if (sum < 1e-10F) {
        return 0.0F;
    }

    /* WHAT: Normalize values to probability distribution */
    /* Using VLA for small counts, heap for large */
    float* probs = NULL;
    float stack_probs[256];
    if (count <= 256) {
        probs = stack_probs;
    } else {
        probs = (float*)nimcp_calloc(count, sizeof(float));
        if (!probs) {
            return 0.0F;
        }
    }

    for (uint32_t i = 0; i < count; i++) {
        probs[i] = fabsf(values[i]) / (fabsf(sum) > 1e-7f ? sum : 1e-7f);
    }

    /* WHAT: Delegate entropy computation to central statistics module */
    float entropy = nimcp_stats_entropy(probs, count);

    if (count > 256) {
        nimcp_free(probs);
        probs = NULL;
    }

    return entropy;
}

/**
 * WHAT: Compute cosine similarity between two vectors
 * WHY: Compare state vectors for similarity
 * HOW: dot(a,b) / (||a|| * ||b||)
 *
 * COMPLEXITY: O(d) where d = dimension
 */


// Forward declarations for static functions (SRP split)
static void bio_broadcast_state_change(introspection_context_t ctx, float cognitive_load, float confidence);
static uint32_t* clone_neurons_per_layer(const uint32_t* source, uint32_t num_layers);
static network_topology_t clone_topology(const network_topology_t* source);
static network_topology_t build_topology(brain_t brain);
static void activity_history_add(introspection_context_t context, const activity_history_entry_t* entry);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_introspection_part_processing.c"  // 3 functions: processing
#include "nimcp_introspection_part_helpers.c"  // 9 functions: helpers
#include "nimcp_introspection_part_accessors.c"  // 19 functions: accessors
#include "nimcp_introspection_part_lifecycle.c"  // 9 functions: lifecycle
#include "nimcp_introspection_part_core.c"  // 10 functions: core
