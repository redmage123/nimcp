//=============================================================================
// nimcp_brain_init_core.c - Core Brain Initialization Functions
//=============================================================================
/**
 * @file nimcp_brain_init_core.c
 * @brief Core brain allocation and network creation
 *
 * WHAT: Brain structure allocation and basic initialization
 * WHY:  Separates core allocation from subsystem initialization
 * HOW:  Provides functions for brain allocation, network creation, labels, and event bus
 *
 * EXTRACTED FROM: nimcp_brain_init.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_core.h"
#include "core/brain/factory/init/nimcp_brain_init_config.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/events/nimcp_event_bus.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/containers/nimcp_hash_table.h"

/* Forward declaration for mesh integration - include avoided to prevent type conflicts */
struct mesh_brain_integration;
typedef struct mesh_brain_integration mesh_brain_integration_t;

/* Mesh brain integration function declarations */
extern nimcp_error_t mesh_brain_integration_register_bbb(
    mesh_brain_integration_t* integration, void* bbb, void* health_agent);
extern nimcp_error_t mesh_brain_integration_register_immune_system(
    mesh_brain_integration_t* integration, void* immune, void* health_agent);
extern nimcp_error_t mesh_brain_integration_register_fep_orchestrator(
    mesh_brain_integration_t* integration, void* fep, void* health_agent);
extern nimcp_error_t mesh_brain_integration_register_working_memory(
    mesh_brain_integration_t* integration, void* wm, void* health_agent);
extern nimcp_error_t mesh_brain_integration_register_executive(
    mesh_brain_integration_t* integration, void* exec, void* health_agent);
extern nimcp_error_t mesh_brain_integration_register_global_workspace(
    mesh_brain_integration_t* integration, void* gw, void* health_agent);
extern nimcp_error_t mesh_brain_integration_register_plasticity_coordinator(
    mesh_brain_integration_t* integration, void* plasticity, void* health_agent);
extern nimcp_error_t mesh_brain_integration_register_bio_async_orchestrator(
    mesh_brain_integration_t* integration, void* bio_orch, void* health_agent);
extern nimcp_error_t mesh_brain_integration_register_hippocampus(
    mesh_brain_integration_t* integration, void* hippocampus, void* health_agent);
extern nimcp_error_t mesh_brain_integration_register_amygdala(
    mesh_brain_integration_t* integration, void* amygdala, void* health_agent);
extern nimcp_error_t mesh_brain_integration_register_thalamus(
    mesh_brain_integration_t* integration, void* thalamus, void* health_agent);
extern nimcp_error_t mesh_brain_integration_register_basal_ganglia(
    mesh_brain_integration_t* integration, void* basal_ganglia, void* health_agent);

/* Mesh brain integration stats structure */
typedef struct mesh_brain_integration_stats {
    size_t memory_modules_registered;
    size_t cognitive_modules_registered;
    size_t sensory_modules_registered;
    size_t motor_modules_registered;
    size_t security_modules_registered;
    size_t plasticity_modules_registered;
    size_t glial_modules_registered;
    size_t orchestrator_modules_registered;
    size_t total_modules_registered;
    size_t registration_failures;
    uint64_t last_registration_time_ns;
} mesh_brain_integration_stats_t;

extern nimcp_error_t mesh_brain_integration_get_stats(
    const mesh_brain_integration_t* integration,
    mesh_brain_integration_stats_t* stats);

#define LOG_MODULE "BRAIN_INIT_CORE"
#include "utils/thread/nimcp_thread_rand.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_core, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

//=============================================================================
// Brain Allocation
//=============================================================================

brain_t nimcp_brain_factory_allocate_brain(void)
{
    brain_t brain = nimcp_calloc(1, sizeof(struct brain_struct));
    if (!brain) {
        set_error("Failed to allocate brain structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain is NULL");

        return NULL;
    }

    brain->last_input = NULL;
    brain->cached_decision = NULL;
    brain->input_size = 0;

    /* Dale's principle: default ON.
     * Re-enabled 2026-04-30 with proper thread safety: brain_enforce_dales_law
     * acquires the plasticity-coordinator mutex for the full sweep, so
     * structural plasticity can't realloc outgoing.overflow mid-iteration.
     * Skips the sweep entirely if the coordinator is absent (e.g., mid-load),
     * avoiding the previous unsynchronized-race SIGSEGV. */
    brain->enforce_dales_law = true;
    brain->neuromod_plasticity_bridge = NULL;

    // Initialize cache mutex for thread-safe access
    if (nimcp_platform_mutex_init(&brain->cache_mutex, false) != 0) {
        set_error("Failed to initialize cache mutex");
        nimcp_free(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_brain_factory_allocate_brain: validation failed");
        return NULL;
    }

    brain->distributed = NULL;  // Initialize as standalone brain

    // Phase 11: Initialize long-term memory consolidation buffer
    brain->longterm_capacity = 100;  // Store up to 100 consolidated memories
    brain->longterm_count = 0;
    brain->longterm_memory = nimcp_calloc(brain->longterm_capacity,
                                          sizeof(*brain->longterm_memory));
    // Guard: If allocation fails, set capacity to 0 (consolidation will be disabled)
    if (!brain->longterm_memory) {
        brain->longterm_capacity = 0;
    }

    // Initialize COW fields
    brain->is_cow_clone = false;
    brain->owns_network = true;  // By default, brain owns its network
    brain->original_network = NULL;
    brain->network_is_cached = false;

    // Phase 3: Initialize reference counting fields
    brain->network_refcount_atomic = NULL;
    brain->can_use_readonly = false;

    // Community Detection: Initialize fields
    brain->functional_modules = NULL;
    brain->network_hubs = NULL;
    brain->topology_metrics = NULL;
    brain->auto_detect_communities = false;
    brain->community_detection_interval = 0.0F;  // Manual only by default

    return brain;
}

//=============================================================================
// Network Creation
//=============================================================================

adaptive_network_t nimcp_brain_factory_create_brain_network(uint32_t num_inputs, uint32_t num_outputs,
                                               uint32_t num_neurons, float sparsity_target,
                                               ode_integration_method_t integration_method)
{
    adaptive_network_config_t net_config =
        nimcp_brain_factory_build_network_config(num_inputs, num_outputs, num_neurons, sparsity_target, integration_method);

    // Guard: Check if layer_sizes allocation failed in build_base_network_config
    if (!net_config.base_config.layer_sizes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_factory_create_brain_network: net_config is NULL");
        return NULL;
    }

    // Skip dense O(N*M) wiring for large networks — use sparse init instead
    bool needs_sparse_wiring = (num_neurons > 5000);
    if (needs_sparse_wiring) {
        net_config.base_config.skip_layer_wiring = true;
    }

    // Default wiring threads for parallel backbone wiring (overridable via brain_config)
    if (net_config.base_config.wiring_threads == 0) {
        net_config.base_config.wiring_threads = 4;
    }

    adaptive_network_t network = adaptive_network_create(&net_config);

    // Free our copy of layer_sizes - adaptive_network_create makes its own deep copy
    if (net_config.base_config.layer_sizes) {
        nimcp_free((void*)net_config.base_config.layer_sizes);
    }

    // For large networks, wiring is deferred to training — connections form
    // organically via plasticity. This avoids millions of pool allocations
    // that bottleneck on the memory tracking system.
    if (network && needs_sparse_wiring) {
        LOG_INFO("Large network (%u neurons) — initial wiring deferred to training", num_neurons);
    }

    return network;
}

//=============================================================================
// Output Labels
//=============================================================================

bool nimcp_brain_factory_init_output_labels(brain_t brain, uint32_t num_outputs)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_output_labels: brain is NULL");
        return false;
    }
    if (num_outputs == 0) {
        // Zero outputs - no allocation needed, but set to NULL
        brain->output_labels = NULL;
        brain->num_output_labels = 0;
        return true;
    }
    brain->output_labels = nimcp_calloc(num_outputs, sizeof(char*));
    if (!brain->output_labels) {
        set_error("Failed to allocate output labels");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_factory_init_output_labels: brain->output_labels is NULL");
        return false;
    }
    brain->num_output_labels = 0;

    // Create O(1) label lookup hash table (FNV-1a)
    hash_table_config_t ht_config = {
        .initial_buckets = 256,
        .key_type = HASH_KEY_STRING,
        .hash_algorithm = HASH_ALG_FNV1A,
        .case_insensitive = false,
        .thread_safe = false  // caller uses brain->cache_mutex
    };
    brain->label_index = hash_table_create(&ht_config);
    // Non-fatal if NULL — falls back to linear search in get_or_create_label_index

    return true;
}

//=============================================================================
// Event Bus Initialization
//=============================================================================

bool nimcp_brain_factory_init_event_bus(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_event_bus: brain is NULL");
        return false;
    }

    // Check if already initialized
    if (brain->event_bus) {
        return true;  // Already initialized
    }

    // Create event bus with immediate delivery (synchronous for predictability)
    brain->event_bus = event_bus_create("brain_event_bus", EVENT_DELIVERY_IMMEDIATE);
    if (!brain->event_bus) {
        set_error("Failed to create brain event bus");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_event_bus: brain->event_bus is NULL");
        return false;
    }

    LOG_INFO("Universal event bus initialized for brain");
    return true;
}

//=============================================================================
// Mesh Network Integration (Phase 15: Brain-Mesh Integration)
//=============================================================================

/**
 * @brief Global mesh brain integration handle for brain module registration
 *
 * Set via nimcp_brain_factory_set_mesh_integration() before brain creation.
 * When set, brain modules are automatically registered with the mesh network.
 */
static mesh_brain_integration_t* g_mesh_brain_integration = NULL;

/**
 * @brief Set the mesh brain integration handle for automatic registration
 *
 * WHAT: Configures brain factory to auto-register modules with mesh
 * WHY:  Enables real brain modules to participate in mesh consensus
 * HOW:  Stores integration handle, used during brain init
 *
 * @param integration Mesh brain integration handle (NULL to disable)
 */
void nimcp_brain_factory_set_mesh_integration(mesh_brain_integration_t* integration) {
    g_mesh_brain_integration = integration;
    if (integration) {
        LOG_INFO("Mesh brain integration enabled for brain factory");
    } else {
        LOG_INFO("Mesh brain integration disabled for brain factory");
    }
}

/**
 * @brief Get the current mesh brain integration handle
 *
 * @return Current integration handle or NULL if not set
 */
mesh_brain_integration_t* nimcp_brain_factory_get_mesh_integration(void) {
    return g_mesh_brain_integration;
}

/**
 * @brief Register brain modules with mesh network
 *
 * WHAT: Registers all available brain modules with the mesh network
 * WHY:  Enables real module instances to participate in mesh consensus
 * HOW:  Iterates brain subsystems, registers non-NULL modules
 *
 * BIOLOGICAL MOTIVATION:
 * - Brain regions form a distributed consensus network
 * - Each region has specialized receptive fields (what it responds to)
 * - Thalamus acts as gateway, amygdala has veto power
 * - Hippocampus is required for memory transactions
 *
 * @param brain Brain instance with initialized modules
 * @return true if registration successful, false on error
 */
bool nimcp_brain_factory_register_with_mesh(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_register_with_mesh: brain is NULL");
        return false;
    }

    mesh_brain_integration_t* integration = g_mesh_brain_integration;
    if (!integration) {
        /* Mesh integration not configured - not an error */
        return true;
    }

    brain_init_core_heartbeat("mesh_registration", 0.0f);

    LOG_INFO("Registering brain modules with mesh network...");

    size_t registered = 0;
    size_t failed = 0;

    /* Security modules (highest priority - must be registered first) */
    if (brain->bbb_system) {
        if (mesh_brain_integration_register_bbb(
                integration, brain->bbb_system, NULL) == NIMCP_SUCCESS) {
            registered++;
        } else {
            failed++;
        }
    }

    if (brain->immune_system) {
        if (mesh_brain_integration_register_immune_system(
                integration, brain->immune_system, NULL) == NIMCP_SUCCESS) {
            registered++;
        } else {
            failed++;
        }
    }

    brain_init_core_heartbeat("mesh_registration", 0.2f);

    /* Core cognitive modules */
    if (brain->fep_orchestrator) {
        if (mesh_brain_integration_register_fep_orchestrator(
                integration, brain->fep_orchestrator, NULL) == NIMCP_SUCCESS) {
            registered++;
        } else {
            failed++;
        }
    }

    if (brain->working_memory) {
        if (mesh_brain_integration_register_working_memory(
                integration, brain->working_memory, NULL) == NIMCP_SUCCESS) {
            registered++;
        } else {
            failed++;
        }
    }

    if (brain->executive) {
        if (mesh_brain_integration_register_executive(
                integration, brain->executive, NULL) == NIMCP_SUCCESS) {
            registered++;
        } else {
            failed++;
        }
    }

    brain_init_core_heartbeat("mesh_registration", 0.4f);

    /* Global workspace for conscious access */
    if (brain->global_workspace) {
        if (mesh_brain_integration_register_global_workspace(
                integration, brain->global_workspace, NULL) == NIMCP_SUCCESS) {
            registered++;
        } else {
            failed++;
        }
    }

    brain_init_core_heartbeat("mesh_registration", 0.6f);

    /* Plasticity coordinator */
    if (brain->plasticity_coordinator) {
        if (mesh_brain_integration_register_plasticity_coordinator(
                integration, brain->plasticity_coordinator, NULL) == NIMCP_SUCCESS) {
            registered++;
        } else {
            failed++;
        }
    }

    /* Bio-async orchestrator */
    if (brain->bio_async_orchestrator) {
        if (mesh_brain_integration_register_bio_async_orchestrator(
                integration, brain->bio_async_orchestrator, NULL) == NIMCP_SUCCESS) {
            registered++;
        } else {
            failed++;
        }
    }

    brain_init_core_heartbeat("mesh_registration", 0.7f);

    /* Subcortical and memory modules */
    if (brain->hippocampus) {
        if (mesh_brain_integration_register_hippocampus(
                integration, brain->hippocampus, NULL) == NIMCP_SUCCESS) {
            registered++;
        } else {
            failed++;
        }
    }

    /* Note: Thalamus is accessed via bridges, not as a standalone subsystem */

    if (brain->basal_ganglia) {
        if (mesh_brain_integration_register_basal_ganglia(
                integration, brain->basal_ganglia, NULL) == NIMCP_SUCCESS) {
            registered++;
        } else {
            failed++;
        }
    }

    /* Amygdala is accessed through emotional system */
    if (brain->emotional_system) {
        /* Get amygdala from emotional system if available */
        void* amygdala = NULL;
        /* Note: Amygdala registration uses emotional_system as the amygdala proxy
         * since amygdala is tightly coupled with emotional processing */
        if (mesh_brain_integration_register_amygdala(
                integration, brain->emotional_system, NULL) == NIMCP_SUCCESS) {
            registered++;
        } else {
            failed++;
        }
    }

    brain_init_core_heartbeat("mesh_registration", 0.8f);

    /* Get statistics */
    mesh_brain_integration_stats_t stats;
    if (mesh_brain_integration_get_stats(integration, &stats) == NIMCP_SUCCESS) {
        LOG_INFO("Mesh registration complete: %zu total (%zu memory, %zu cognitive, "
                 "%zu security, %zu plasticity), %zu failed",
                 stats.total_modules_registered,
                 stats.memory_modules_registered,
                 stats.cognitive_modules_registered,
                 stats.security_modules_registered,
                 stats.plasticity_modules_registered,
                 failed);
    } else {
        LOG_INFO("Mesh registration complete: %zu registered, %zu failed",
                 registered, failed);
    }

    brain_init_core_heartbeat("mesh_registration", 1.0f);

    return (failed == 0);
}
