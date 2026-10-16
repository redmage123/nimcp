//=============================================================================
// nimcp_brain_core.h - Brain Core Allocation and Initialization
//=============================================================================

#ifndef NIMCP_BRAIN_CORE_H
#define NIMCP_BRAIN_CORE_H

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// Brain allocation and initialization
brain_t allocate_brain(void);
adaptive_network_t create_brain_network(uint32_t num_inputs, uint32_t num_outputs,
                                        uint32_t num_neurons, float sparsity_target,
                                        ode_integration_method_t integration_method);
bool init_output_labels(brain_t brain, uint32_t num_outputs);
bool init_attention_subsystem(brain_t brain);
bool init_brain_regions_subsystem(brain_t brain);
bool init_symbolic_logic_subsystem(brain_t brain);
bool init_symbolic_reasoning_subsystem(brain_t brain);
bool init_epistemic_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_CORE_H
