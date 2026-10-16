//=============================================================================
// nimcp_brain_init_core.h - Core Brain Initialization Functions
//=============================================================================
/**
 * @file nimcp_brain_init_core.h
 * @brief Core brain allocation and network creation functions
 *
 * WHAT: Brain structure allocation and basic initialization
 * WHY:  Separates core allocation from subsystem initialization
 * HOW:  Provides functions for brain allocation, network creation, labels, and event bus
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#ifndef NIMCP_BRAIN_INIT_CORE_H
#define NIMCP_BRAIN_INIT_CORE_H

#include "core/brain/nimcp_brain.h"
#include "plasticity/adaptive/nimcp_adaptive.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for mesh integration - full type in nimcp_mesh_brain_integration.h */
struct mesh_brain_integration;
typedef struct mesh_brain_integration mesh_brain_integration_t;

/**
 * @brief Allocate brain structure and initialize basic fields
 *
 * WHAT: Allocates brain struct and initializes cache, memory, and COW fields
 * WHY:  Separates allocation from subsystem initialization
 * HOW:  Allocates struct, initializes mutex, allocates longterm memory
 *
 * @return Allocated brain structure or NULL on error
 */
brain_t nimcp_brain_factory_allocate_brain(void);

/**
 * @brief Create adaptive network for brain
 *
 * WHAT: Creates neural network with adaptive configuration
 * WHY:  Isolates network creation complexity
 * HOW:  Builds config, creates network, frees temporary allocations
 *
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param num_neurons Neuron count
 * @param sparsity_target Target sparsity
 * @param integration_method ODE integration method
 * @return Network handle or NULL on error
 */
adaptive_network_t nimcp_brain_factory_create_brain_network(
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t num_neurons,
    float sparsity_target,
    ode_integration_method_t integration_method);

/**
 * @brief Initialize output labels array
 *
 * @param brain Brain to initialize
 * @param num_outputs Number of output labels
 * @return true on success
 */
bool nimcp_brain_factory_init_output_labels(brain_t brain, uint32_t num_outputs);

/**
 * @brief Initialize universal event bus
 *
 * WHAT: Creates event bus for brain-wide event coordination
 * WHY:  Enables all modules to publish and subscribe to events
 * HOW:  Creates event bus with immediate delivery mode
 *
 * @param brain Brain to initialize event bus for
 * @return true if initialization successful, false on error
 */
bool nimcp_brain_factory_init_event_bus(brain_t brain);

/* ============================================================================
 * Mesh Network Integration (Phase 15: Brain-Mesh Integration)
 * ============================================================================ */

/**
 * @brief Set mesh brain integration for automatic module registration
 *
 * WHAT: Configures brain factory to auto-register modules with mesh
 * WHY:  Enables real brain modules to participate in mesh consensus
 * HOW:  Stores integration handle, used during brain init
 *
 * Call this before creating brains to enable mesh integration.
 * After brain creation, modules will be automatically registered.
 *
 * @param integration Mesh brain integration handle (NULL to disable)
 */
void nimcp_brain_factory_set_mesh_integration(mesh_brain_integration_t* integration);

/**
 * @brief Get current mesh brain integration handle
 *
 * @return Current integration handle or NULL if not set
 */
mesh_brain_integration_t* nimcp_brain_factory_get_mesh_integration(void);

/**
 * @brief Register brain modules with mesh network
 *
 * WHAT: Registers all available brain modules with the mesh network
 * WHY:  Enables real module instances to participate in mesh consensus
 * HOW:  Iterates brain subsystems, registers non-NULL modules
 *
 * BRAIN MODULES REGISTERED:
 * - Security: BBB (blood-brain barrier), immune system
 * - Cognitive: FEP orchestrator, working memory, executive controller
 * - Memory: Hippocampus (if present via hippocampus_context)
 * - Plasticity: Plasticity coordinator
 * - System: Bio-async orchestrator, global workspace
 *
 * Each module is assigned:
 * - Mesh adapter category (cognitive, memory, security, etc.)
 * - Receptive field based on brain region (for pattern-based routing)
 * - Endorser role (REQUIRED, PREFERRED, or OPTIONAL)
 * - Health agent wiring (if configured)
 *
 * @param brain Brain instance with initialized modules
 * @return true if registration successful, false on error
 */
bool nimcp_brain_factory_register_with_mesh(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_CORE_H
