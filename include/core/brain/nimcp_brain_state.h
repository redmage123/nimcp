//=============================================================================
// nimcp_brain_state.h - Brain State Accessors and COW Handling
//=============================================================================

#ifndef NIMCP_BRAIN_STATE_H
#define NIMCP_BRAIN_STATE_H

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// State accessors
adaptive_network_t brain_get_network(brain_t brain);
neuromodulator_system_t brain_get_neuromodulator_system(brain_t brain);
sleep_system_t brain_get_sleep_system(brain_t brain);
theory_of_mind_t brain_get_theory_of_mind(brain_t brain);
explanation_generator_t brain_get_explanation_generator(brain_t brain);

// COW handling
bool ensure_writable_network(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_STATE_H
