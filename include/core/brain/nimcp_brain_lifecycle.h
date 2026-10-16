//=============================================================================
// nimcp_brain_lifecycle.h - Brain Lifecycle Management Interface
//=============================================================================
/**
 * @file nimcp_brain_lifecycle.h
 * @brief Brain creation, destruction, initialization, and reset functions
 *
 * RESPONSIBILITY: Managing brain lifecycle from creation to destruction
 */

#ifndef NIMCP_BRAIN_LIFECYCLE_H
#define NIMCP_BRAIN_LIFECYCLE_H

#include "core/brain/nimcp_brain.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "plasticity/adaptive/nimcp_adaptive.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct task_strategy task_strategy_t;

//=============================================================================
// Brain Allocation and Network Creation
//=============================================================================

/**
 * @brief Allocate and initialize brain structure
 * @return Allocated brain or NULL on error
 */
brain_t allocate_brain(void);

/**
 * @brief Create adaptive network for brain
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param num_neurons Neuron count
 * @param sparsity_target Target sparsity
 * @param integration_method ODE integration method
 * @return Network handle or NULL on error
 */
adaptive_network_t create_brain_network(uint32_t num_inputs, uint32_t num_outputs,
                                        uint32_t num_neurons, float sparsity_target,
                                        ode_integration_method_t integration_method);

//=============================================================================
// Configuration and Statistics Initialization
//=============================================================================

/**
 * @brief Initialize brain configuration with strategy
 * @param config Output config structure
 * @param task_name Name for brain
 * @param size Size preset
 * @param task Task type
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param strategy Task strategy for learning rate
 */
void init_brain_config(brain_config_t* config, const char* task_name, brain_size_t size,
                      brain_task_t task, uint32_t num_inputs, uint32_t num_outputs,
                      task_strategy_t* strategy);

/**
 * @brief Initialize brain statistics
 * @param stats Output stats structure
 * @param task_name Name for brain
 * @param size Size preset
 * @param num_inputs Input dimension
 * @param learning_rate Learning rate
 */
void init_brain_stats(brain_stats_t* stats, const char* task_name, brain_size_t size,
                     uint32_t num_inputs, float learning_rate);

//=============================================================================
// Subsystem Initialization
//=============================================================================

/**
 * @brief Initialize output labels array
 * @param brain Brain to initialize
 * @param num_outputs Number of output labels
 * @return true on success
 */
bool init_output_labels(brain_t brain, uint32_t num_outputs);

/**
 * @brief Initialize multihead attention mechanism
 * @param brain Brain to initialize
 * @return true on success, false on error
 */
bool init_attention_subsystem(brain_t brain);

/**
 * @brief Initialize brain regions subsystem
 * @param brain Brain to initialize
 * @return true on success, false on error
 */
bool init_brain_regions_subsystem(brain_t brain);

/**
 * @brief Initialize symbolic logic subsystem
 * @param brain Brain to initialize
 * @return true on success, false on error
 */
bool init_symbolic_logic_subsystem(brain_t brain);

/**
 * @brief Initialize symbolic reasoning subsystem
 * @param brain Brain to initialize
 * @return true on success, false on error
 */
bool init_symbolic_reasoning_subsystem(brain_t brain);

/**
 * @brief Initialize epistemic filtering subsystem
 * @param brain Brain to initialize
 * @return true on success, false on error
 */
bool init_epistemic_subsystem(brain_t brain);

//=============================================================================
// Brain Destruction
//=============================================================================

/**
 * @brief Destroy brain and free all resources
 * @param brain Brain to destroy
 */
void brain_destroy(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_LIFECYCLE_H
