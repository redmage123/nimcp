//=============================================================================
// nimcp_neuron_model_internal.h - Internal Neuron Model Structure
//=============================================================================
/**
 * @file nimcp_neuron_model_internal.h
 * @brief Internal structure definition for neuron model implementations
 *
 * WHAT: Full definition of neuron_model_state_struct with flexible array member
 * WHY: Allows model implementations to access model_state[] while keeping public API opaque
 * HOW: Private header included only by neuron model implementation files
 *
 * DESIGN PATTERN: Opaque Pointer with Internal Access
 * - Public API uses forward declaration (nimcp_neuron_model.h)
 * - Implementation uses full definition (this file)
 * - Maintains encapsulation while allowing flexible array access
 *
 * USAGE:
 * - Include in neuron_model.c and all model implementations (izhikevich.c, etc)
 * - DO NOT include in public headers or external code
 *
 * @author NIMCP Development Team
 * @date 2025-11-06
 */

#ifndef NIMCP_NEURON_MODEL_INTERNAL_H
#define NIMCP_NEURON_MODEL_INTERNAL_H

#include "core/neuron_models/nimcp_neuron_model.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Complete neuron model state structure
 *
 * WHAT: Full definition with flexible array member for model-specific state
 * WHY: Allows type-safe access to model_state by implementations
 * HOW: Flexible array member allocated with total_size = sizeof(struct) + state_size
 *
 * MEMORY LAYOUT:
 * [ vtable ptr | model_state[0] | model_state[1] | ... | model_state[N-1] ]
 * |<-- sizeof(struct) -->|<------------- state_size --------------->|
 *
 * COMPLEXITY: O(1) access to both vtable and model_state
 */
struct neuron_model_state_struct {
    const neuron_model_vtable_t* vtable; /**< Virtual function table pointer */
    uint8_t model_state[];               /**< Flexible array member for model-specific state */
};

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_NEURON_MODEL_INTERNAL_H
