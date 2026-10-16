/**
 * @file nimcp_mesh_brain_integration.h
 * @brief Brain Module Registration Integration for Mesh Network
 *
 * WHAT: Connects real brain module instances to the mesh network
 * WHY:  Replace dummy pointers with actual brain components for true integration
 * HOW:  Registration macros and functions that wire brain modules to mesh
 *
 * USAGE:
 * ```c
 * // Initialize mesh brain integration
 * mesh_brain_integration_config_t config;
 * mesh_brain_integration_default_config(&config);
 * mesh_brain_integration_t* integration = mesh_brain_integration_create(
 *     bootstrap, &config);
 *
 * // Register brain modules (typically called from brain init)
 * MESH_REGISTER_BRAIN_MODULE(integration, brain->hippocampus);
 * MESH_REGISTER_COGNITIVE_MODULE(integration, brain->working_memory);
 *
 * // Or use the brain-wide registration function
 * mesh_brain_integration_register_brain(integration, brain);
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                    MESH BRAIN INTEGRATION LAYER                             │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                         BRAIN INSTANCE                                │   │
 * │  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐    │   │
 * │  │  │Hippocampus│ │ Amygdala │ │ Thalamus │ │ Immune  │ │ FEP Orch │    │   │
 * │  │  └─────┬─────┘ └─────┬────┘ └────┬─────┘ └────┬────┘ └─────┬────┘    │   │
 * │  └────────│─────────────│───────────│────────────│────────────│─────────┘   │
 * │           │             │           │            │            │              │
 * │           ▼             ▼           ▼            ▼            ▼              │
 * │  ┌────────────────────────────────────────────────────────────────────────┐ │
 * │  │              MESH BRAIN INTEGRATION                                    │ │
 * │  │  • Type-safe registration via module registry                         │ │
 * │  │  • Receptive field assignment based on brain region                   │ │
 * │  │  • Health agent wiring for distributed monitoring                     │ │
 * │  │  • Automatic category assignment (memory, security, cognitive, etc.)  │ │
 * │  └────────────────────────────────────────────────────────────────────────┘ │
 * │                                    │                                        │
 * │                                    ▼                                        │
 * │  ┌────────────────────────────────────────────────────────────────────────┐ │
 * │  │                        MESH BOOTSTRAP                                  │ │
 * │  │  ┌──────────────┐  ┌───────────────┐  ┌─────────────────┐             │ │
 * │  │  │Module Registry│  │Pattern Router │  │ Health Bridge   │             │ │
 * │  │  └──────────────┘  └───────────────┘  └─────────────────┘             │ │
 * │  └────────────────────────────────────────────────────────────────────────┘ │
 * │                                                                              │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_BRAIN_INTEGRATION_H
#define NIMCP_MESH_BRAIN_INTEGRATION_H

#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_module_registry.h"
#include "mesh/nimcp_mesh_receptive_fields.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_brain_integration mesh_brain_integration_t;

/* Forward declaration for brain_t - defined in nimcp_brain.h */
struct brain_struct;
typedef struct brain_struct* brain_t;

/*
 * NOTE: Brain module types are NOT forward-declared here to avoid conflicts
 * with types already defined in nimcp_brain_internal.h.
 *
 * The registration functions use void* for opaque module pointers.
 * Type-safe registration is achieved through the registration macros
 * that extract sizeof() and _MAGIC at compile time.
 */

/* Forward declare brain_immune_system_t only - other types use void* */
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;

/* ============================================================================
 * Brain Region Types for Registration
 * ============================================================================ */

/**
 * @brief Brain region categories for mesh registration
 */
typedef enum mesh_brain_region {
    MESH_BRAIN_REGION_UNKNOWN = 0,

    /* Memory regions */
    MESH_BRAIN_REGION_HIPPOCAMPUS,
    MESH_BRAIN_REGION_EPISODIC_MEMORY,
    MESH_BRAIN_REGION_SEMANTIC_MEMORY,
    MESH_BRAIN_REGION_WORKING_MEMORY,
    MESH_BRAIN_REGION_PROCEDURAL_MEMORY,

    /* Limbic regions */
    MESH_BRAIN_REGION_AMYGDALA,
    MESH_BRAIN_REGION_HYPOTHALAMUS,
    MESH_BRAIN_REGION_NUCLEUS_ACCUMBENS,
    MESH_BRAIN_REGION_CINGULATE,

    /* Cortical regions */
    MESH_BRAIN_REGION_PFC_LEFT,
    MESH_BRAIN_REGION_PFC_RIGHT,
    MESH_BRAIN_REGION_DORSOLATERAL_PFC,
    MESH_BRAIN_REGION_ORBITOFRONTAL,
    MESH_BRAIN_REGION_ANTERIOR_CINGULATE,

    /* Motor regions */
    MESH_BRAIN_REGION_MOTOR_CORTEX,
    MESH_BRAIN_REGION_PREMOTOR,
    MESH_BRAIN_REGION_SUPPLEMENTARY_MOTOR,
    MESH_BRAIN_REGION_CEREBELLUM,
    MESH_BRAIN_REGION_BASAL_GANGLIA,

    /* Sensory regions */
    MESH_BRAIN_REGION_VISUAL_CORTEX,
    MESH_BRAIN_REGION_AUDITORY_CORTEX,
    MESH_BRAIN_REGION_SOMATOSENSORY,
    MESH_BRAIN_REGION_THALAMUS,

    /* Cognitive regions */
    MESH_BRAIN_REGION_FEP_ORCHESTRATOR,
    MESH_BRAIN_REGION_ATTENTION,
    MESH_BRAIN_REGION_REASONING,
    MESH_BRAIN_REGION_PLANNING,
    MESH_BRAIN_REGION_EXECUTIVE,
    MESH_BRAIN_REGION_GLOBAL_WORKSPACE,
    MESH_BRAIN_REGION_THEORY_OF_MIND,

    /* Security regions */
    MESH_BRAIN_REGION_BBB,
    MESH_BRAIN_REGION_IMMUNE_SYSTEM,
    MESH_BRAIN_REGION_THREAT_DETECTOR,

    /* Plasticity regions */
    MESH_BRAIN_REGION_STDP,
    MESH_BRAIN_REGION_LTP,
    MESH_BRAIN_REGION_HOMEOSTATIC,
    MESH_BRAIN_REGION_PLASTICITY_COORDINATOR,

    /* Glial regions */
    MESH_BRAIN_REGION_ASTROCYTE,
    MESH_BRAIN_REGION_OLIGODENDROCYTE,

    /* Orchestrators */
    MESH_BRAIN_REGION_BIO_ASYNC_ORCHESTRATOR,

    MESH_BRAIN_REGION_COUNT
} mesh_brain_region_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Brain integration configuration
 */
typedef struct mesh_brain_integration_config {
    /* Registration options */
    bool auto_register_available;       /**< Auto-register modules found in brain */
    bool register_with_health_agents;   /**< Wire health agents for registered modules */
    bool register_receptive_fields;     /**< Assign receptive fields to modules */

    /* Logging */
    bool verbose_logging;

} mesh_brain_integration_config_t;

/**
 * @brief Brain integration statistics
 */
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

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default integration configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_brain_integration_default_config(
    mesh_brain_integration_config_t* config
);

/**
 * @brief Create brain integration handler
 *
 * @param bootstrap Mesh bootstrap handle (required)
 * @param config Configuration (NULL for defaults)
 * @return Integration handle or NULL on failure
 */
mesh_brain_integration_t* mesh_brain_integration_create(
    mesh_bootstrap_t* bootstrap,
    const mesh_brain_integration_config_t* config
);

/**
 * @brief Destroy brain integration handler
 *
 * @param integration Integration to destroy (NULL-safe)
 */
void mesh_brain_integration_destroy(mesh_brain_integration_t* integration);

/**
 * @brief Initialize global mesh brain integration system
 *
 * Called once at system startup before any brain registration.
 *
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_brain_integration_init(void);

/**
 * @brief Cleanup global mesh brain integration system
 */
void mesh_brain_integration_cleanup(void);

/* ============================================================================
 * Brain-Wide Registration API
 * ============================================================================ */

/**
 * @brief Register all available modules from a brain instance
 *
 * Scans the brain structure and registers all non-NULL modules with the mesh.
 * This is the preferred way to integrate an entire brain at once.
 *
 * @param integration Brain integration handle
 * @param brain Brain instance to register
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_brain_integration_register_brain(
    mesh_brain_integration_t* integration,
    brain_t brain
);

/**
 * @brief Unregister all modules from a brain instance
 *
 * @param integration Brain integration handle
 * @param brain Brain instance to unregister
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_brain_integration_unregister_brain(
    mesh_brain_integration_t* integration,
    brain_t brain
);

/* ============================================================================
 * Individual Module Registration API
 * ============================================================================ */

/**
 * @brief Register a brain module by region type
 *
 * @param integration Brain integration handle
 * @param region Brain region type
 * @param module Module instance pointer
 * @param module_size sizeof(module_type) for validation
 * @param module_magic Magic number for type validation
 * @param health_agent Optional health agent (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_brain_integration_register_module(
    mesh_brain_integration_t* integration,
    mesh_brain_region_t region,
    void* module,
    size_t module_size,
    uint32_t module_magic,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Unregister a module by region type
 *
 * @param integration Brain integration handle
 * @param region Brain region type
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_brain_integration_unregister_module(
    mesh_brain_integration_t* integration,
    mesh_brain_region_t region
);

/* ============================================================================
 * Specific Module Registration Functions
 * ============================================================================ */

/**
 * @brief Register hippocampus with mesh
 */
nimcp_error_t mesh_brain_integration_register_hippocampus(
    mesh_brain_integration_t* integration,
    void* hippocampus,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Register amygdala with mesh (VETO role)
 */
nimcp_error_t mesh_brain_integration_register_amygdala(
    mesh_brain_integration_t* integration,
    void* amygdala,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Register thalamus with mesh (GATEWAY role)
 */
nimcp_error_t mesh_brain_integration_register_thalamus(
    mesh_brain_integration_t* integration,
    void* thalamus,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Register basal ganglia with mesh (MOTOR role)
 */
nimcp_error_t mesh_brain_integration_register_basal_ganglia(
    mesh_brain_integration_t* integration,
    void* basal_ganglia,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Register brain immune system with mesh
 */
nimcp_error_t mesh_brain_integration_register_immune_system(
    mesh_brain_integration_t* integration,
    brain_immune_system_t* immune,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Register BBB with mesh (security gateway)
 */
nimcp_error_t mesh_brain_integration_register_bbb(
    mesh_brain_integration_t* integration,
    void* bbb,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Register FEP orchestrator with mesh
 */
nimcp_error_t mesh_brain_integration_register_fep_orchestrator(
    mesh_brain_integration_t* integration,
    void* fep_orchestrator,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Register working memory with mesh
 */
nimcp_error_t mesh_brain_integration_register_working_memory(
    mesh_brain_integration_t* integration,
    void* working_memory,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Register executive controller with mesh
 */
nimcp_error_t mesh_brain_integration_register_executive(
    mesh_brain_integration_t* integration,
    void* executive,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Register global workspace with mesh
 */
nimcp_error_t mesh_brain_integration_register_global_workspace(
    mesh_brain_integration_t* integration,
    void* workspace,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Register plasticity coordinator with mesh
 */
nimcp_error_t mesh_brain_integration_register_plasticity_coordinator(
    mesh_brain_integration_t* integration,
    void* plasticity,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Register bio-async orchestrator with mesh
 */
nimcp_error_t mesh_brain_integration_register_bio_async_orchestrator(
    mesh_brain_integration_t* integration,
    void* bio_async_orch,
    nimcp_health_agent_t* health_agent
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get mesh participant ID for a brain region
 *
 * @param integration Brain integration handle
 * @param region Brain region type
 * @return Participant ID or 0 if not registered
 */
mesh_participant_id_t mesh_brain_integration_get_participant_id(
    const mesh_brain_integration_t* integration,
    mesh_brain_region_t region
);

/**
 * @brief Check if a brain region is registered
 *
 * @param integration Brain integration handle
 * @param region Brain region type
 * @return true if registered
 */
bool mesh_brain_integration_is_registered(
    const mesh_brain_integration_t* integration,
    mesh_brain_region_t region
);

/**
 * @brief Get integration statistics
 *
 * @param integration Brain integration handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_brain_integration_get_stats(
    const mesh_brain_integration_t* integration,
    mesh_brain_integration_stats_t* stats
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get mesh adapter category for a brain region
 *
 * @param region Brain region type
 * @return Corresponding mesh adapter category
 */
mesh_adapter_category_t mesh_brain_region_to_category(mesh_brain_region_t region);

/**
 * @brief Get receptive field for a brain region
 *
 * @param region Brain region type
 * @return Receptive field or NULL if not defined
 */
const mesh_receptive_field_t* mesh_brain_region_get_receptive_field(
    mesh_brain_region_t region
);

/**
 * @brief Get human-readable name for a brain region
 *
 * @param region Brain region type
 * @return Region name string
 */
const char* mesh_brain_region_to_string(mesh_brain_region_t region);

/* ============================================================================
 * Registration Macros
 * ============================================================================ */

/**
 * @brief Register a brain module with automatic type extraction
 *
 * Requires that module_type has _MAGIC defined.
 *
 * @param integration Brain integration handle
 * @param module_ptr Pointer to module instance
 * @param module_type Module struct type
 * @param region Brain region type
 */
#define MESH_REGISTER_BRAIN_MODULE(integration, module_ptr, module_type, region) \
    mesh_brain_integration_register_module( \
        (integration), \
        (region), \
        (module_ptr), \
        sizeof(module_type), \
        module_type##_MAGIC, \
        NULL \
    )

/**
 * @brief Register a brain module with health agent
 */
#define MESH_REGISTER_BRAIN_MODULE_WITH_HEALTH(integration, module_ptr, module_type, region, health) \
    mesh_brain_integration_register_module( \
        (integration), \
        (region), \
        (module_ptr), \
        sizeof(module_type), \
        module_type##_MAGIC, \
        (health) \
    )

/**
 * @brief Register a cognitive module (convenience macro)
 */
#define MESH_REGISTER_COGNITIVE_MODULE(integration, module_ptr, module_type, region) \
    MESH_REGISTER_BRAIN_MODULE(integration, module_ptr, module_type, region)

/**
 * @brief Register a memory module (convenience macro)
 */
#define MESH_REGISTER_MEMORY_MODULE(integration, module_ptr, module_type, region) \
    MESH_REGISTER_BRAIN_MODULE(integration, module_ptr, module_type, region)

/**
 * @brief Register a security module (convenience macro)
 */
#define MESH_REGISTER_SECURITY_MODULE(integration, module_ptr, module_type, region) \
    MESH_REGISTER_BRAIN_MODULE(integration, module_ptr, module_type, region)

/**
 * @brief Register a plasticity module (convenience macro)
 */
#define MESH_REGISTER_PLASTICITY_MODULE(integration, module_ptr, module_type, region) \
    MESH_REGISTER_BRAIN_MODULE(integration, module_ptr, module_type, region)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_BRAIN_INTEGRATION_H */
