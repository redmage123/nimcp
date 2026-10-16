/**
 * @file nimcp_api_network.c
 * @brief Neural network API implementation
 *
 * This module handles standalone neural network operations
 * (separate from full brain structures).
 *
 * Responsibilities:
 * - Neural network creation and destruction
 * - Network forward pass
 * - Network training (placeholder)
 */

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "API_NETWORK"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(api_network)

/* API Exception Integration (Phase 7) */
extern void set_error(const char* fmt, ...);
#define NIMCP_API_SET_ERROR(fmt, ...) set_error(fmt, ##__VA_ARGS__)
#include "api/nimcp_api_exception.h"

#include "nimcp.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"
#include "security/nimcp_bbb_helpers.h"
#include <stdio.h>
#include <string.h>

//=============================================================================
// External References (from nimcp.c)
//=============================================================================

// These functions are defined in nimcp.c and shared across modules
extern void set_error(const char* fmt, ...);
extern const char* nimcp_get_error(void);

//=============================================================================
// Network API Implementation
//=============================================================================

nimcp_network_t nimcp_network_create(
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t num_hidden,
    float learning_rate)
{
    // Allocate handle
    nimcp_network_t handle = (nimcp_network_t)nimcp_malloc(sizeof(struct nimcp_network_handle));
    if (!handle) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(struct nimcp_network_handle),
            "Failed to allocate network handle");
        return NULL;
    }

    // Create config for internal API
    network_config_t config = {0};
    config.input_size = num_inputs;
    config.output_size = num_outputs;
    // Calculate total neurons: inputs + hidden layers + outputs
    config.num_neurons = num_inputs + num_hidden + num_outputs;
    config.learning_rate = learning_rate;

    // Create internal neural network
    handle->internal_network = neural_network_create(&config);

    if (!handle->internal_network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create internal neural network");
        nimcp_free(handle);
        return NULL;
    }

    return handle;
}

void nimcp_network_destroy(nimcp_network_t network) {
    if (!network) {
        return;
    }

    if (network->internal_network) {
        neural_network_destroy(network->internal_network);
    }

    nimcp_free(network);
}

nimcp_status_t nimcp_network_forward(
    nimcp_network_t network,
    const float* inputs,
    uint32_t num_inputs,
    float* outputs,
    uint32_t num_outputs)
{
    NIMCP_CHECK_THROW(network, NIMCP_ERROR_NULL_ARG, "Network handle is NULL in network_forward");
    NIMCP_CHECK_THROW(inputs, NIMCP_ERROR_NULL_ARG, "Inputs array is NULL in network_forward");
    NIMCP_CHECK_THROW(outputs, NIMCP_ERROR_NULL_ARG, "Outputs array is NULL in network_forward");

    // BBB validation: validate input buffer
    if (!bbb_validate_buffer_access(inputs, 0, num_inputs * sizeof(float),
                                    num_inputs * sizeof(float), "nimcp_network_forward")) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_INPUT, "BBB validation failed for inputs buffer");
        return NIMCP_ERROR_INVALID;
    }

    // Call internal network API
    bool success = neural_network_forward(network->internal_network,
                                         inputs, num_inputs,
                                         outputs, num_outputs);

    if (!success) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Network forward pass failed");
        return NIMCP_ERROR;
    }

    return NIMCP_OK;
}

nimcp_status_t nimcp_network_train(
    nimcp_network_t network,
    const float* inputs,
    uint32_t num_inputs,
    const float* targets,
    uint32_t num_targets)
{
    NIMCP_CHECK_THROW(network, NIMCP_ERROR_NULL_ARG, "Network handle is NULL in network_train");

    // Training not yet implemented in internal API
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Network training not yet implemented");
    return NIMCP_ERROR;
}
