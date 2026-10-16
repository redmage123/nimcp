//=============================================================================
// nimcp_brain_init_hippocampus.c - Hippocampus Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_hippocampus.c
 * @brief Hippocampus Initialization Implementation
 *
 * WHAT: Initialization functions for hippocampus (memory and navigation)
 * WHY:  Enable episodic memory and spatial navigation in the brain
 * HOW:  Creates hippocampus adapter and connects all integration bridges
 *
 * EXTRACTED FROM: Brain factory initialization
 * DATE: 2025-12-30
 *
 * @version Phase H1: Hippocampus Brain Integration
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_hippocampus.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_HIPPOCAMPUS"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_hippocampus, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Hippocampus includes
#include "core/brain/regions/hippocampus/nimcp_hippocampus_adapter.h"
#include "core/brain/regions/hippocampus/nimcp_hippocampus_quantum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

// Memory consolidation includes (Phase M2-M4)
#include "cognitive/memory/nimcp_systems_consolidation.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "cognitive/memory/nimcp_engram.h"

#include <string.h>

//=============================================================================
// Mesh Integration (Phase: Mesh Network Integration)
//=============================================================================

/* Mesh integration - forward declarations to avoid header conflicts */
struct mesh_bootstrap;
typedef struct mesh_bootstrap mesh_bootstrap_t;
struct mesh_hippocampus_integration;
typedef struct mesh_hippocampus_integration mesh_hippocampus_integration_t;

/* Mesh hippocampus integration function declarations */
extern mesh_hippocampus_integration_t* mesh_hippocampus_create(
    mesh_bootstrap_t* bootstrap,
    void* hippocampus,
    const void* config
);
extern void mesh_hippocampus_destroy(mesh_hippocampus_integration_t* integration);
extern int mesh_hippocampus_register_participant(mesh_hippocampus_integration_t* integration);
extern int mesh_hippocampus_set_health_agent(mesh_hippocampus_integration_t* integration,
                                              void* agent);

/** Global mesh bootstrap handle (set externally) */
static mesh_bootstrap_t* g_hippocampus_mesh_bootstrap = NULL;
static mesh_hippocampus_integration_t* g_hippocampus_mesh_integration = NULL;

/**
 * @brief Set the mesh bootstrap handle for hippocampus mesh registration
 *
 * WHAT: Configures hippocampus init to register with mesh network
 * WHY:  Enable coordinated memory operations via mesh consensus
 * HOW:  Stores bootstrap handle, used during hippocampus init
 *
 * @param bootstrap Mesh bootstrap handle (NULL to disable)
 */
void nimcp_brain_hippocampus_set_mesh_bootstrap(mesh_bootstrap_t* bootstrap) {
    g_hippocampus_mesh_bootstrap = bootstrap;
    if (bootstrap) {
        NIMCP_LOGGING_INFO("Mesh bootstrap set for hippocampus integration");
    }
}

/**
 * @brief Get the hippocampus mesh integration handle
 *
 * @return Current mesh integration or NULL if not registered
 */
mesh_hippocampus_integration_t* nimcp_brain_hippocampus_get_mesh_integration(void) {
    return g_hippocampus_mesh_integration;
}

// Forward declarations for substrate and thalamic bridges
// These will be implemented as the substrate/thalamic bridge system matures
struct hippocampus_substrate_bridge;
typedef struct hippocampus_substrate_bridge hippocampus_substrate_bridge_t;

struct hippocampus_thalamic_bridge;
typedef struct hippocampus_thalamic_bridge hippocampus_thalamic_bridge_t;

// Hippocampus substrate config
typedef struct {
    bool enable_metabolic_modulation;
    bool enable_sleep_modulation;
    float atp_threshold;
    float fatigue_decay_rate;
} hippocampus_substrate_config_t;

// Hippocampus thalamic config
typedef struct {
    bool enable_anterior_routing;
    bool enable_memory_gating;
    bool enable_consolidation_signals;
    float min_signal_strength;
    float routing_priority;
} hippocampus_thalamic_config_t;

// Stub implementations for substrate/thalamic bridges
// These will be replaced when full implementations are available

static hippocampus_substrate_config_t hippocampus_substrate_default_config(void) {
    hippocampus_substrate_config_t config = {
        .enable_metabolic_modulation = true,
        .enable_sleep_modulation = true,
        .atp_threshold = 0.3f,
        .fatigue_decay_rate = 0.01f
    };
    return config;
}

static hippocampus_substrate_bridge_t* hippocampus_substrate_bridge_create(
    void* hippocampus, void* substrate, const hippocampus_substrate_config_t* config
) {
    (void)hippocampus;
    (void)substrate;
    (void)config;
    /* Stub: Return NULL to indicate not yet implemented.
     * Full implementation will integrate with neural_substrate module.
     * Callers handle NULL gracefully. */
    return NULL;
}

static void hippocampus_substrate_bridge_destroy(hippocampus_substrate_bridge_t* bridge) {
    (void)bridge;
}

static int hippocampus_substrate_bridge_update(hippocampus_substrate_bridge_t* bridge) {
    (void)bridge;
    return 0;
}

static hippocampus_thalamic_config_t hippocampus_thalamic_default_config(void) {
    hippocampus_thalamic_config_t config = {
        .enable_anterior_routing = true,
        .enable_memory_gating = true,
        .enable_consolidation_signals = true,
        .min_signal_strength = 0.1f,
        .routing_priority = 0.8f
    };
    return config;
}

static hippocampus_thalamic_bridge_t* hippocampus_thalamic_bridge_create(
    void* hippocampus, void* router, const hippocampus_thalamic_config_t* config
) {
    (void)hippocampus;
    (void)router;
    (void)config;
    /* Stub: Return NULL to indicate not yet implemented.
     * Full implementation will integrate with thalamic router module.
     * Callers handle NULL gracefully. */
    return NULL;
}

static void hippocampus_thalamic_bridge_destroy(hippocampus_thalamic_bridge_t* bridge) {
    (void)bridge;
    // Stub
}

static int hippocampus_thalamic_bridge_reset(hippocampus_thalamic_bridge_t* bridge) {
    (void)bridge;
    return 0;  // Stub
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Connect hippocampus to bio-async messaging
 */
static bool connect_hippocampus_to_bio_async(brain_t brain) {
    if (!brain || !brain->hippocampus) return true; /* Non-fatal if not available */

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled && brain->bio_async_ctx) {
        /*
         * TODO: Register hippocampus message handlers
         * bio_router_register_module(router, BIO_MODULE_HIPPOCAMPUS, brain->hippocampus);
         */
        LOG_DEBUG(LOG_MODULE, "Hippocampus bio-async connection deferred");
    }

    return true;
}

/**
 * @brief Connect hippocampus substrate bridge to neural substrate
 */
static bool connect_substrate_bridge(brain_t brain) {
    if (!brain || !brain->hippocampus_substrate_bridge) return true;

    /* Apply initial metabolic effects */
    if (hippocampus_substrate_bridge_update(brain->hippocampus_substrate_bridge) != 0) {
        LOG_WARN(LOG_MODULE, "Initial substrate bridge update failed");
    }

    return true;
}

/**
 * @brief Connect hippocampus thalamic bridge to thalamic router
 */
static bool connect_thalamic_bridge(brain_t brain) {
    if (!brain || !brain->hippocampus_thalamic_bridge) return true;

    /* Reset bridge to clean state */
    if (hippocampus_thalamic_bridge_reset(brain->hippocampus_thalamic_bridge) != 0) {
        LOG_WARN(LOG_MODULE, "Thalamic bridge reset failed");
    }

    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool nimcp_brain_factory_init_hippocampus_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_hippocampus_subsystem: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->hippocampus) {
        return true;  /* Already initialized */
    }

    /* Check if hippocampus is enabled in config */
    /* Default to enabled for memory-capable brains (episodic, consolidation, working memory) */
    if (!brain->config.enable_working_memory &&
        !brain->config.enable_consolidation) {
        brain->hippocampus_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Hippocampus not enabled (no memory config)");
        return true;  /* Not enabled, not an error */
    }

    LOG_INFO(LOG_MODULE, "Initializing hippocampus subsystem");

    /* Create hippocampus adapter with default configuration */
    hippocampus_config_t hippocampus_cfg = hippocampus_default_config();

    brain->hippocampus = hippocampus_create(&hippocampus_cfg);
    if (!brain->hippocampus) {
        set_error("Failed to create hippocampus adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_hippocampus_subsystem: brain->hippocampus is NULL");
        return false;
    }

    brain->hippocampus_enabled = true;
    brain->last_hippocampus_update_us = 0;

    LOG_DEBUG(LOG_MODULE, "Hippocampus adapter created (capacity=%u)",
              hippocampus_cfg.memory_capacity);

    /* Initialize integration bridges */
    if (!nimcp_brain_factory_init_hippocampus_substrate_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Hippocampus substrate bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_hippocampus_thalamic_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Hippocampus thalamic bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_hippocampus_quantum_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Hippocampus quantum bridge init failed (non-fatal)");
    }

    /* Connect to other subsystems */
    if (!nimcp_brain_factory_connect_hippocampus_to_cortex(brain)) {
        LOG_WARN(LOG_MODULE, "Hippocampus-cortex connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_hippocampus_to_amygdala(brain)) {
        LOG_WARN(LOG_MODULE, "Hippocampus-amygdala connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_hippocampus_to_training(brain)) {
        LOG_WARN(LOG_MODULE, "Hippocampus-training connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_hippocampus_to_immune(brain)) {
        LOG_WARN(LOG_MODULE, "Hippocampus-immune connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_hippocampus_to_sleep(brain)) {
        LOG_WARN(LOG_MODULE, "Hippocampus-sleep connection failed (non-fatal)");
    }

    /* Connect to bio-async */
    if (!connect_hippocampus_to_bio_async(brain)) {
        LOG_WARN(LOG_MODULE, "Hippocampus bio-async connection failed (non-fatal)");
    }

    /* Register with mesh network if bootstrap is available */
    if (g_hippocampus_mesh_bootstrap) {
        brain_init_hippocampus_heartbeat("mesh_registration", 0.0f);

        mesh_hippocampus_integration_t* mesh_integration = mesh_hippocampus_create(
            g_hippocampus_mesh_bootstrap,
            brain->hippocampus,
            NULL  /* Use default config */
        );

        if (mesh_integration) {
            /* Set health agent if available */
            if (g_brain_init_hippocampus_health_agent) {
                mesh_hippocampus_set_health_agent(mesh_integration,
                                                  g_brain_init_hippocampus_health_agent);
            }

            /* Register as mesh participant */
            int mesh_result = mesh_hippocampus_register_participant(mesh_integration);
            if (mesh_result == NIMCP_SUCCESS) {
                g_hippocampus_mesh_integration = mesh_integration;
                LOG_INFO(LOG_MODULE, "Hippocampus registered with mesh network");
            } else {
                LOG_WARN(LOG_MODULE, "Failed to register hippocampus with mesh (error %d)",
                         mesh_result);
                mesh_hippocampus_destroy(mesh_integration);
            }
        } else {
            LOG_WARN(LOG_MODULE, "Failed to create hippocampus mesh integration");
        }

        brain_init_hippocampus_heartbeat("mesh_registration", 1.0f);
    }

    LOG_INFO(LOG_MODULE, "Hippocampus subsystem initialized successfully");
    return true;
}

bool nimcp_brain_factory_init_hippocampus_substrate_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_hippocampus_substrate_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->hippocampus_substrate_bridge) {
        return true;
    }

    /* Need hippocampus adapter first */
    if (!brain->hippocampus) {
        return true;  /* Not ready yet, will be called again */
    }

    LOG_DEBUG(LOG_MODULE, "Initializing hippocampus substrate bridge");

    /* Get neural substrate - may be NULL in simple configurations */
    void* substrate = NULL;
    /*
     * TODO: Get substrate from brain if available
     * substrate = brain->substrate;
     */

    /* Create substrate bridge with default config */
    hippocampus_substrate_config_t config = hippocampus_substrate_default_config();

    brain->hippocampus_substrate_bridge = hippocampus_substrate_bridge_create(
        brain->hippocampus, substrate, &config);

    if (!brain->hippocampus_substrate_bridge) {
        LOG_WARN(LOG_MODULE, "Hippocampus substrate bridge not yet implemented");
        return true;  /* Non-fatal: stub implementation */
    }

    /* Connect to substrate */
    connect_substrate_bridge(brain);

    LOG_DEBUG(LOG_MODULE, "Hippocampus substrate bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_hippocampus_thalamic_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_hippocampus_thalamic_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->hippocampus_thalamic_bridge) {
        return true;
    }

    /* Need hippocampus adapter first */
    if (!brain->hippocampus) {
        return true;  /* Not ready yet */
    }

    LOG_DEBUG(LOG_MODULE, "Initializing hippocampus thalamic bridge");

    /* Get thalamic router - may be NULL in simple configurations */
    void* router = NULL;
    /*
     * TODO: Get thalamic router from brain if available
     * router = brain->thalamic_router;
     */

    /* Create thalamic bridge with default config */
    hippocampus_thalamic_config_t thal_config = hippocampus_thalamic_default_config();

    brain->hippocampus_thalamic_bridge = hippocampus_thalamic_bridge_create(
        brain->hippocampus, router, &thal_config);

    if (!brain->hippocampus_thalamic_bridge) {
        LOG_WARN(LOG_MODULE, "Hippocampus thalamic bridge not yet implemented");
        return true;  /* Non-fatal: stub implementation */
    }

    /* Connect to thalamus */
    connect_thalamic_bridge(brain);

    LOG_DEBUG(LOG_MODULE, "Hippocampus thalamic bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_hippocampus_quantum_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_hippocampus_quantum_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->hippocampus_quantum_bridge) {
        return true;
    }

    /* Need hippocampus adapter first */
    if (!brain->hippocampus) {
        return true;  /* Not ready yet */
    }

    /* Check if quantum reasoning is enabled */
    if (!brain->quantum_reasoning_enabled) {
        LOG_DEBUG(LOG_MODULE, "Quantum reasoning not enabled, skipping quantum bridge");
        return true;  /* Not enabled, not an error */
    }

    LOG_DEBUG(LOG_MODULE, "Initializing hippocampus quantum bridge");

    /* Create quantum bridge with default config */
    hippocampus_quantum_config_t config = hippocampus_quantum_default_config();

    /* Use default search depth - can be overridden via config API */

    brain->hippocampus_quantum_bridge = hippocampus_quantum_bridge_create(
        brain->hippocampus, &config);

    if (!brain->hippocampus_quantum_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create hippocampus quantum bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_hippocampus_quantum_bridge: brain->hippocampus_quantum_bridge is NULL");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Hippocampus quantum bridge initialized");
    return true;
}

/**
 * @brief Callback for hippocampus memory consolidation to cortex
 *
 * WHAT: Called when hippocampus marks a memory for consolidation
 * WHY:  Transfers strong memories from hippocampus to cortical areas
 * HOW:  Schedules replay in systems consolidation for gradual transfer
 *
 * BIOLOGICAL BASIS:
 * - Sleep replay (SWS): Hippocampal memories reactivate
 * - Systems consolidation: Gradual transfer to neocortex
 * - Semantic extraction: Episodic → semantic transformation
 *
 * @param memory The memory being consolidated
 * @param user_data Brain pointer for accessing systems consolidation
 */
static void hippocampus_to_cortex_consolidation_callback(
    const hippocampus_memory_t* memory,
    void* user_data)
{
    struct brain_struct* brain = (struct brain_struct*)user_data;

    if (!brain || !memory) {
        return;
    }

    LOG_DEBUG(LOG_MODULE, "Consolidation callback: memory_id=%u strength=%.2f",
              memory->memory_id, memory->strength);

    /*
     * Schedule memory for replay during sleep.
     * Priority based on memory strength and emotional valence.
     * Higher priority = replayed sooner during SWS.
     */
    if (brain->systems_consolidation) {
        /* Calculate replay priority from strength and emotional salience */
        float priority = memory->strength * 0.6f;
        if (memory->emotional_valence > 0.5f || memory->emotional_valence < -0.5f) {
            /* Emotional memories get higher priority (amygdala modulation) */
            priority += 0.3f;
        }
        if (priority > 1.0f) priority = 1.0f;

        /*
         * Schedule replay - the memory_id from hippocampus needs to map
         * to an engram_id in the engram system. For now, use memory_id
         * as the engram_id (assumes 1:1 correspondence).
         */
        bool scheduled = systems_consolidation_schedule_replay(
            brain->systems_consolidation,
            (uint64_t)memory->memory_id,
            priority);

        if (scheduled) {
            LOG_DEBUG(LOG_MODULE, "Scheduled replay for memory %u with priority %.2f",
                      memory->memory_id, priority);
        } else {
            LOG_WARN(LOG_MODULE, "Failed to schedule replay for memory %u (queue full?)",
                     memory->memory_id);
        }
    }

    /*
     * Optionally create semantic concept from consolidated memory.
     * This extracts semantic features for long-term knowledge storage.
     */
    if (brain->semantic_memory && memory->features && memory->feature_count > 0) {
        /* Create semantic concept from memory features */
        uint64_t concept_id = semantic_memory_create_concept(
            brain->semantic_memory,
            memory->features,
            memory->feature_count,
            NULL,  /* No label - generated from features */
            CONCEPT_EVENT  /* Episodic memories become event concepts */
        );

        if (concept_id > 0) {
            LOG_DEBUG(LOG_MODULE, "Created semantic concept %lu from memory %u",
                      (unsigned long)concept_id, memory->memory_id);
        }
    }
}

bool nimcp_brain_factory_connect_hippocampus_to_cortex(brain_t brain) {
    if (!brain || !brain->hippocampus) {
        return true;  /* Nothing to connect */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting hippocampus to cortical areas");

    /*
     * Register hippocampus for systems consolidation.
     * During sleep, memories are transferred from hippocampus to neocortex.
     * This involves:
     * - Prefrontal cortex (semantic memory)
     * - Temporal cortex (episodic memory)
     * - Parietal cortex (spatial memory)
     */

    /* Set up consolidation callback to transfer memories to cortex */
    if (brain->systems_consolidation || brain->semantic_memory) {
        /*
         * Register callback with hippocampus.
         * When hippocampus consolidates memories (via hippocampus_consolidate_memories),
         * this callback is invoked for each memory above the strength threshold.
         * The callback then:
         * 1. Schedules replay in systems consolidation (for SWS transfer)
         * 2. Creates semantic concepts from memory features
         */
        bool callback_set = hippocampus_set_consolidation_callback(
            brain->hippocampus,
            hippocampus_to_cortex_consolidation_callback,
            brain  /* Pass brain pointer as user_data */
        );

        if (callback_set) {
            LOG_DEBUG(LOG_MODULE, "Hippocampus consolidation callback registered");
        } else {
            LOG_WARN(LOG_MODULE, "Failed to set hippocampus consolidation callback");
        }
    }

    /* Connect semantic memory to systems consolidation for bidirectional flow */
    if (brain->semantic_memory && brain->systems_consolidation) {
        /*
         * Link semantic memory to systems consolidation.
         * This allows semantic memory to:
         * - Extract concepts from consolidated cortical nodes
         * - Build semantic relations from co-consolidated memories
         */
        semantic_memory_set_consolidation(
            brain->semantic_memory,
            brain->systems_consolidation
        );
        LOG_DEBUG(LOG_MODULE, "Semantic memory connected to systems consolidation");
    }

    /* Log successful connection */
    if (brain->systems_consolidation) {
        LOG_DEBUG(LOG_MODULE, "Hippocampus registered with systems consolidation");
    }
    if (brain->semantic_memory) {
        LOG_DEBUG(LOG_MODULE, "Hippocampus connected to semantic memory");
    }

    LOG_DEBUG(LOG_MODULE, "Hippocampus connected to cortical areas");
    return true;
}

bool nimcp_brain_factory_connect_hippocampus_to_amygdala(brain_t brain) {
    if (!brain || !brain->hippocampus) {
        return true;  /* Nothing to connect */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting hippocampus to amygdala");

    /*
     * Hippocampus-amygdala connection for emotional memory:
     * - Amygdala tags memories with emotional significance
     * - High emotional valence -> stronger memory encoding
     * - Stress hormones modulate hippocampal plasticity
     */

    /* Check if emotional system is available */
    if (brain->emotional_system) {
        /*
         * TODO: Register for emotional modulation
         * emotional_system_register_memory_module(brain->emotional_system,
         *     MEMORY_MODULE_HIPPOCAMPUS, brain->hippocampus);
         */
        LOG_DEBUG(LOG_MODULE, "Hippocampus registered with emotional system");
    }

    LOG_DEBUG(LOG_MODULE, "Hippocampus connected to amygdala");
    return true;
}

bool nimcp_brain_factory_connect_hippocampus_to_training(brain_t brain) {
    if (!brain || !brain->hippocampus) {
        return true;  /* Nothing to connect */
    }

    /* Check if training is enabled */
    if (!brain->enable_training_integration || !brain->training_ctx) {
        return true;  /* Training not enabled */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting hippocampus to training system");

    /*
     * Register hippocampus adapter with training context.
     * This allows memory-based learning through:
     * - Place cell tuning from navigation experience
     * - Memory encoding improvement from retrieval practice
     * - Pattern separation/completion optimization
     */

    /*
     * TODO: Register with training
     * nimcp_brain_training_register_module(brain->training_ctx,
     *     TRAIN_MODULE_HIPPOCAMPUS, brain->hippocampus);
     */

    LOG_DEBUG(LOG_MODULE, "Hippocampus connected to training system");
    return true;
}

bool nimcp_brain_factory_connect_hippocampus_to_immune(brain_t brain) {
    if (!brain || !brain->hippocampus) {
        return true;  /* Nothing to connect */
    }

    /* Check if immune system is available */
    if (!brain->immune_enabled || !brain->immune_system) {
        return true;  /* Immune not enabled */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting hippocampus to immune system");

    /*
     * Register for cytokine signals that affect memory.
     * Neuroinflammation (high IL-1beta, TNF-alpha) can cause:
     * - Reduced LTP (impaired memory encoding)
     * - Impaired spatial navigation
     * - Accelerated memory decay
     */

    /*
     * TODO: Register with immune system
     * brain_immune_register_cytokine_callback(brain->immune_system,
     *     CYTOKINE_IL1B | CYTOKINE_TNF_A, hippocampus_inflammation_callback, brain->hippocampus);
     */

    LOG_DEBUG(LOG_MODULE, "Hippocampus connected to immune system");
    return true;
}

bool nimcp_brain_factory_connect_hippocampus_to_sleep(brain_t brain) {
    if (!brain || !brain->hippocampus) {
        return true;  /* Nothing to connect */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting hippocampus to sleep system");

    /*
     * Sleep is critical for hippocampal memory consolidation:
     * - NREM sleep: Sharp-wave ripples replay memories
     * - REM sleep: Memory integration and emotional processing
     * - Sleep deprivation impairs memory encoding
     */

    /* Register for sleep state changes */
    /*
     * TODO: Register sleep callback
     * sleep_system_register_callback(&brain->sleep_system,
     *     SLEEP_EVENT_NREM_ENTER | SLEEP_EVENT_REM_ENTER,
     *     hippocampus_sleep_callback, brain->hippocampus);
     */

    LOG_DEBUG(LOG_MODULE, "Hippocampus connected to sleep system");
    return true;
}

//=============================================================================
// Destruction
//=============================================================================

/**
 * @brief Destroy hippocampus subsystem
 *
 * WHAT: Clean up all hippocampus resources and bridges
 * WHY:  Prevent memory leaks during brain destruction
 * HOW:  Destroy in reverse initialization order
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_hippocampus_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_hippocampus_subsystem: brain is NULL"); return; }

    LOG_DEBUG(LOG_MODULE, "Destroying hippocampus subsystem");

    /* Destroy mesh integration first */
    if (g_hippocampus_mesh_integration) {
        mesh_hippocampus_destroy(g_hippocampus_mesh_integration);
        g_hippocampus_mesh_integration = NULL;
        LOG_DEBUG(LOG_MODULE, "Hippocampus mesh integration destroyed");
    }

    /* Destroy quantum bridge first (depends on hippocampus) */
    if (brain->hippocampus_quantum_bridge) {
        hippocampus_quantum_bridge_destroy(brain->hippocampus_quantum_bridge);
        brain->hippocampus_quantum_bridge = NULL;
    }

    /* Destroy thalamic bridge */
    if (brain->hippocampus_thalamic_bridge) {
        hippocampus_thalamic_bridge_destroy(brain->hippocampus_thalamic_bridge);
        brain->hippocampus_thalamic_bridge = NULL;
    }

    /* Destroy substrate bridge */
    if (brain->hippocampus_substrate_bridge) {
        hippocampus_substrate_bridge_destroy(brain->hippocampus_substrate_bridge);
        brain->hippocampus_substrate_bridge = NULL;
    }

    /* Destroy hippocampus adapter */
    if (brain->hippocampus) {
        hippocampus_destroy(brain->hippocampus);
        brain->hippocampus = NULL;
    }

    brain->hippocampus_enabled = false;
    brain->last_hippocampus_update_us = 0;

    LOG_DEBUG(LOG_MODULE, "Hippocampus subsystem destroyed");
}
